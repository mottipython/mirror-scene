#!/usr/bin/env python3
"""Turn the Gemini drawings in assets/src into the pictures the screensaver builds
in, under assets/art.

    tools/make_sprites.py

groucho.jpeg is a 4 x 4 sheet on flat magenta of the man in the nightshirt, facing
right: four walking frames, then standing, peering, eyebrows up, cigar out, arms up,
kick, turning, crouching, waving, holding a hat, putting it on, grinning in it.
others.jpeg has the curly-haired one (standing, horn, dropping his hat, grinning),
the third man (two walking frames, standing, shrugging), and a hat, a horn and a
cigar. The falling hat by him is left out. Those figures face right on the sheet and are turned to face left here.
room.jpeg is the room.

The drawings are found as connected shapes (an arm raised over the row above does
not matter), and every figure frame is written to one layout: he stands 480 high,
his feet 10 above the bottom edge, and the middle of his body in the middle across.
Needs ImageMagick and numpy.
"""
import subprocess
import numpy as np

SRC, OUT = "assets/src", "assets/art"
# the inside of the empty frame in the room picture: tools/compose_room.py put it there
from compose_room import OPENING

def load(path):
    w, h = map(int, subprocess.check_output(["magick", "identify", "-format", "%w %h", path]).split())
    raw = subprocess.check_output(["magick", path, "-depth", "8", "RGB:-"])
    return np.frombuffer(raw, np.uint8).reshape(h, w, 3).astype(np.float32)

def key(img):
    """Magenta background to alpha, and take its colour cast off the edges."""
    r, g, b = img[..., 0].copy(), img[..., 1].copy(), img[..., 2].copy()
    spill = np.minimum(r, b) - g
    cut = np.clip(spill, 0, None) * 0.92
    r, b = r - cut, b - cut
    alpha = np.clip(1.0 - (spill - 24) / 70.0, 0, 1)
    return np.dstack([r, g, b, alpha * 255]).clip(0, 255).astype(np.uint8)

def label(m):
    """Connected shapes in a small boolean grid (4-neighbour)."""
    lab = np.zeros(m.shape, np.int32)
    n = 0
    for y0, x0 in zip(*np.nonzero(m)):
        if lab[y0, x0]: continue
        n += 1
        stack = [(y0, x0)]
        lab[y0, x0] = n
        while stack:
            y, x = stack.pop()
            for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= yy < m.shape[0] and 0 <= xx < m.shape[1] and m[yy, xx] and not lab[yy, xx]:
                    lab[yy, xx] = n; stack.append((yy, xx))
    return lab, n

def drawings(a, expect, name):
    """The separate drawings on a sheet, in reading order. Small bits (smoke, motion
    marks) go with the nearest large shape."""
    C = 4
    solid = a[..., 3] > 128
    h, w = solid.shape[0] // C, solid.shape[1] // C
    grid = solid[:h * C, :w * C].reshape(h, C, w, C).any(axis=(1, 3))
    grown = grid.copy()                                  # join a figure's own loose bits
    for dy in (-2, -1, 1, 2):
        grown |= np.roll(grid, dy, axis=0)
    for dx in (-2, -1, 1, 2):
        grown |= np.roll(grown, dx, axis=1)
    lab, n = label(grown)
    area = np.bincount(lab.ravel())[1:]
    big = [i + 1 for i in range(n) if area[i] > 0.015 * area.max()]
    boxes = {}
    for i in range(1, n + 1):
        ys, xs = np.nonzero(lab == i)
        boxes[i] = (ys.min(), ys.max(), xs.min(), xs.max())
    owner = {i: i for i in big}
    for i in range(1, n + 1):
        if i in owner: continue
        y0, y1, x0, x1 = boxes[i]
        cy, cx = (y0 + y1) / 2, (x0 + x1) / 2
        def dist(j):
            b = boxes[j]
            return max(b[0] - cy, cy - b[1], 0) + max(b[2] - cx, cx - b[3], 0)
        owner[i] = min(big, key=dist)
    if len(big) != expect:
        raise SystemExit(f"{name}: found {len(big)} drawings, expected {expect}")
    # reading order: rows by where the drawings stand (their bottoms), then across
    bottoms = sorted(big, key=lambda i: boxes[i][1])
    rows, cur = [], [bottoms[0]]
    for i in bottoms[1:]:
        if boxes[i][1] - boxes[cur[-1]][1] > 40: rows.append(cur); cur = [i]
        else: cur.append(i)
    rows.append(cur)
    order = [i for r in rows for i in sorted(r, key=lambda i: boxes[i][2])]
    out = []
    full = np.repeat(np.repeat(lab, C, axis=0), C, axis=1)
    full = np.pad(full, ((0, a.shape[0] - full.shape[0]), (0, a.shape[1] - full.shape[1])))
    for i in order:
        mine = np.isin(full, [j for j, o in owner.items() if o == i])
        piece = a.copy()
        piece[..., 3] = np.where(mine, piece[..., 3], 0)
        ys, xs = np.nonzero(piece[..., 3] > 16)
        out.append(piece[ys.min():ys.max() + 1, xs.min():xs.max() + 1])
    return out

def save(rgba, w, h, path):
    subprocess.run(["magick", "-size", f"{rgba.shape[1]}x{rgba.shape[0]}", "-depth", "8", "RGBA:-",
                    "-filter", "Lanczos", "-resize", f"{w}x{h}!", "-define", "png:compression-level=9", "-define", "png:exclude-chunk=date,time", path],
                   input=np.ascontiguousarray(rgba).tobytes(), check=True)

def frame(p, k, path):
    """One figure at scale k, on a canvas with his feet at the bottom and his body in
    the middle across."""
    solid = p[..., 3] > 128
    hgt = p.shape[0]
    band = solid[int(hgt * 0.35):int(hgt * 0.6)]
    mid = np.nonzero(band)[1].mean()
    half = int(max(mid, p.shape[1] - mid)) + 2
    canvas = np.zeros((hgt, 2 * half, 4), np.uint8)
    x0 = int(half - mid)
    canvas[:, x0:x0 + p.shape[1]] = p
    pad = int(round(10 / k))
    canvas = np.pad(canvas, ((0, pad), (0, 0), (0, 0)))
    w, h = max(1, round(canvas.shape[1] * k)), max(1, round(canvas.shape[0] * k))
    save(canvas, w, h, path)
    print(f"{path.split('/')[-1]:14} {w}x{h}")

def harpo(path_in, path_out):
    """The same frame with Harpo under the nightcap: Groucho's dark hair, which is
    behind his face (the brows and moustache are in front of it), made blond, and a
    bush of blond curls peeking out round the back of his head. The frames face right."""
    from PIL import Image, ImageDraw, ImageFilter
    im = Image.open(path_in).convert("RGBA")
    a = np.asarray(im).astype(int)
    h, w = a.shape[:2]
    solid = a[..., 3] > 128
    rows = np.nonzero(solid.any(axis=1))[0]
    top, bottom = rows.min(), rows.max()
    head = np.zeros_like(solid)
    head[top:top + int((bottom - top) * 0.34)] = True
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    skin = solid & head & (r > 190) & (g > 130) & (g < 215) & (b > 100) & (b < 190) & (r - b > 40)
    if skin.sum() < 30:
        return im.save(path_out)
    ys, xs = np.nonzero(skin)
    fx, fw = np.median(xs), xs.max() - xs.min()
    xx = np.arange(w)[None, :].repeat(h, axis=0)
    dark = solid & head & (np.maximum(np.maximum(r, g), b) < 95) & (xx < fx - fw * 0.12)
    fill = dark & (np.maximum(np.maximum(r, g), b) >= 26)
    out = a.copy()
    k = (np.maximum(np.maximum(r, g), b)[fill] - 26) / 70.0          # keep its light and shade
    out[fill, 0] = (214 + 30 * k).clip(0, 255)
    out[fill, 1] = (168 + 40 * k).clip(0, 255)
    out[fill, 2] = (60 + 40 * k).clip(0, 255)
    # curls behind the head, showing only where they stick out beyond him
    curls = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(curls)
    R = max(4, int((bottom - top) * 0.035))
    dy, dx = np.nonzero(dark)
    if len(dx):
        rng = np.random.default_rng(int(fx * 7 + top))
        blobs = []
        for i in rng.choice(len(dx), size=min(70, len(dx)), replace=False):
            blobs.append((dx[i] + rng.integers(-R, R), dy[i] + rng.integers(-R, R), R * rng.uniform(0.9, 1.4)))
        # inked all round, then filled again a little smaller, so that only the
        # outside edge of the bush keeps its ink, scalloped; and a few curl marks
        ink = max(2, R // 3)
        for cx, cy, rr in blobs:
            d.ellipse([cx - rr - ink, cy - rr - ink, cx + rr + ink, cy + rr + ink], fill=(40, 26, 14, 255))
        for cx, cy, rr in blobs:
            d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=(236, 196, 92, 255))
        for cx, cy, rr in blobs[::3]:
            d.arc([cx - rr * 0.6, cy - rr * 0.6, cx + rr * 0.6, cy + rr * 0.6], 200, 340, fill=(190, 140, 50, 255), width=max(1, ink // 2))
    curls.alpha_composite(Image.fromarray(out.astype(np.uint8), "RGBA"))
    curls.save(path_out)

def main():
    subprocess.run(["mkdir", "-p", OUT], check=True)
    g = drawings(key(load(f"{SRC}/groucho.jpeg")), 16, "groucho")
    names = ["w1", "w2", "w3", "w4", "stand", "peer", "brows", "cigar", "arms", "kick", "spin", "crouch", "wave", "hold", "hat_on", "grin"]
    k = 480 / g[4].shape[0]                        # standing, he is 480 high
    for n, p in zip(names, g):
        frame(p, k, f"{OUT}/g_{n}.png")

    # his reflection, who is Harpo: from harpo.jpeg, the same sheet redrawn with him
    # (see assets/PROMPTS.md), or failing that made from Groucho's frames
    import os
    if os.path.exists(f"{SRC}/harpo.jpeg"):
        hp = drawings(key(load(f"{SRC}/harpo.jpeg")), 16, "harpo")
        k = 480 / hp[4].shape[0]
        for n, p in zip(names, hp):
            frame(p, k, f"{OUT}/hp_{n}.png")
    else:
        for n in names:
            harpo(f"{OUT}/g_{n}.png", f"{OUT}/hp_{n}.png")
    o = drawings(key(load(f"{SRC}/others.jpeg")), 12, "others")
    del o[3]          # the hat he is dropping: the show drops its own, from his hand
    flip = [p[:, ::-1] for p in o[:8]]
    k = 480 / flip[0].shape[0]
    for n, p in zip(["h_stand", "h_horn", "h_drop", "h_grin"], flip[:4]):
        frame(p, k, f"{OUT}/{n}.png")
    k = 480 / flip[6].shape[0]
    for n, p in zip(["c_w1", "c_w2", "c_stand", "c_shrug"], flip[4:8]):
        frame(p, k, f"{OUT}/{n}.png")
    hat = o[8]
    save(hat, 240, max(1, round(hat.shape[0] * 240 / hat.shape[1])), f"{OUT}/hat.png")
    # the room, and the same with the opening in the dividing wall cut out: that one
    # is drawn over whoever is beyond the wall, so he is seen only through the frame
    subprocess.run(["magick", f"{SRC}/room.jpeg", "-resize", "2560x", "-quality", "88", f"{OUT}/room.jpg"], check=True)
    w, h = map(int, subprocess.check_output(["magick", "identify", "-format", "%w %h", f"{OUT}/room.jpg"]).split())
    hole = " ".join(f"{x * w:.1f},{y * h:.1f}" for x, y in OPENING)
    subprocess.run(["magick", f"{OUT}/room.jpg", "(", "-size", f"{w}x{h}", "xc:white", "-fill", "black", "-draw", f"polygon {hole}", ")",
                    "-alpha", "off", "-compose", "copy_opacity", "-composite", "-define", "png:compression-level=9", "-define", "png:exclude-chunk=date,time", f"{OUT}/room_front.png"], check=True)
    print("room")

if __name__ == "__main__":
    main()
