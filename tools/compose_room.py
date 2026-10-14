#!/usr/bin/env python3
"""Build the room, so the mirror is geometrically right whatever Gemini draws:

  - assets/src/bedroom.jpeg, a bedroom with nothing in the middle, is the near room;
  - a dividing wall comes out of its back wall toward us on the right, papered
    with the bedroom's own wallpaper and a little shaded, since it is turned away
    from the lamps;
  - in it, an empty gilt frame, set into the wall in perspective as the opening:
    assets/src/frame.jpeg on magenta if there is one, otherwise drawn here;
  - through the opening, the room beyond: the bedroom flipped left to right, as a
    mirror would show it, a little smaller and dimmer for being further away.

Writes assets/src/room.jpeg. All places are parts of the picture's width and
height. Needs Pillow and numpy.

    tools/compose_room.py
"""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

BEDROOM = "assets/src/bedroom.jpeg"
FRAME = "assets/src/frame.jpeg"
OUT = "assets/src/room.jpeg"
# the inside of the frame in the wall: top-left, top-right, bottom-right, bottom-left
# The wall: its far end meets the back wall at x 0.47, where the panelling meets the
# floor, and it runs off the picture to the right. Its top and foot are straight lines
# that meet at the vanishing point, and the opening's corners lie on lines to it too.
BASE = 0.685                         # the foot of the back wall, down the picture
VP = (0.037, 0.437)                  # where the wall's lines meet
WALL_END = 0.47
def on_wall(x, up):
    """The point on the wall `x` across and `up` of the way from its foot to its top."""
    k = (x - VP[0]) / (WALL_END - VP[0])
    foot = VP[1] + (BASE - VP[1]) * k
    top = VP[1] + (-0.02 - VP[1]) * k
    return (x, foot + (top - foot) * up)
WALL = [on_wall(WALL_END, 1), on_wall(1.02, 1), on_wall(1.02, 0), on_wall(WALL_END, 0)]
OPENING = [on_wall(0.52, 0.58), on_wall(0.70, 0.58), on_wall(0.70, 0), on_wall(0.52, 0)]
PAPER = (0.36, 0.0, 0.64, BASE)      # the back wall between the lamps, panelling and all
WALL_SHADE = 0.8
FAR_SCALE, FAR_DIM = 0.75, 0.86      # the room beyond
FAR_LAMP = 0.6                       # where across the picture its lamp is seen, through the frame
LAMP_AT = 0.31                       # where a lamp is across the bedroom
INK = (42, 28, 18, 255)

def coeffs(src, dst):
    """The 8 numbers PIL's perspective transform wants: for each output point (dst)
    the input point (src) it comes from."""
    A, b = [], []
    for (x, y), (u, v) in zip(dst, src):
        A.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); b.append(u)
        A.append([0, 0, 0, x, y, 1, -v * x, -v * y]); b.append(v)
    return np.linalg.solve(np.array(A, float), np.array(b, float)).tolist()

def keyed(path):
    """A picture on magenta, with the magenta made clear and its cast taken off the edges."""
    a = np.asarray(Image.open(path).convert("RGB")).astype(float)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    spill = np.minimum(r, b) - g
    cut = np.clip(spill, 0, None) * 0.92
    alpha = np.clip(1.0 - (spill - 24) / 70.0, 0, 1)
    out = np.dstack([r - cut, g, b - cut, alpha * 255]).clip(0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")

def inside(frame):
    """The clear inside of a frame: from the middle, out to where the gilt starts."""
    a = np.asarray(frame)[..., 3] > 128
    h, w = a.shape
    cy, cx = h // 2, w // 2
    row, col = a[cy], a[:, cx]
    x0 = max(i for i in range(cx) if row[i]) + 1
    x1 = min(i for i in range(cx, w) if row[i])
    y0 = max(i for i in range(cy) if col[i]) + 1
    y1 = min([i for i in range(cy, h) if col[i]] or [h])   # no bottom bar: the inside goes to the floor
    return x0, y0, x1, y1

def draw_frame(W=1000, H=1560, band=110):
    """An empty gilt frame, drawn: moulded bands of gold with ink between them, a
    rosette at the top corners, and a shell crest on top. It has no bottom bar: its
    sides stand on the floor, like a doorway. The inside is clear."""
    im = Image.new("RGBA", (W, H + 160), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    top = 160
    GOLD = [(120, 78, 20), (196, 146, 52), (232, 188, 86), (252, 226, 150), (214, 166, 64), (150, 104, 32)]
    steps = [0, 0.12, 0.3, 0.45, 0.6, 0.82, 1.0]
    for k, c in enumerate(GOLD):                     # the mouldings, from the outside in
        o = band * steps[k]
        d.rectangle([o, top + o, W - 1 - o, top + H + band], fill=c + (255,), outline=INK, width=4)
    d.rectangle([band, top + band, W - 1 - band, top + H + band], fill=(0, 0, 0, 0), outline=INK, width=7)
    d.rectangle([0, top, W - 1, top + H + band], outline=INK, width=9)
    im = im.crop((0, 0, W, top + H))               # cut level with the floor
    d = ImageDraw.Draw(im)
    for cx, cy in ((band / 2, top + band / 2), (W - band / 2, top + band / 2)):
        r = band * 0.34                               # a rosette at each corner
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=GOLD[3] + (255,), outline=INK, width=5)
        d.ellipse([cx - r * 0.4, cy - r * 0.4, cx + r * 0.4, cy + r * 0.4], fill=GOLD[1] + (255,), outline=INK, width=4)
    cx, cy, R = W / 2, top + 10, 190                  # the shell crest
    d.pieslice([cx - R, cy - R, cx + R, cy + R], 180, 360, fill=GOLD[2] + (255,), outline=INK, width=7)
    for a in range(196, 345, 16):
        t = np.radians(a)
        d.line([(cx, cy), (cx + R * 0.92 * np.cos(t), cy + R * 0.92 * np.sin(t))], fill=INK, width=4)
    d.ellipse([cx - 34, cy - 34, cx + 34, cy + 34], fill=GOLD[3] + (255,), outline=INK, width=5)
    for side in (-1, 1):                              # scrolls either side of it
        sx = cx + side * (R + 70)
        d.arc([sx - 70, cy - 70, sx + 70, cy + 40], 180 if side < 0 else 270, 360 if side < 0 else 180 + 270, fill=INK, width=9)
        d.ellipse([sx - 26, cy - 40, sx + 26, cy + 12], fill=GOLD[2] + (255,), outline=INK, width=5)
    return im

def main():
    room = Image.open(BEDROOM).convert("RGBA")
    w, h = room.size
    clean = room.copy()
    P = lambda quad: [(x * w, y * h) for x, y in quad]
    def mask(quad):
        m = Image.new("L", (w, h), 0)
        ImageDraw.Draw(m).polygon(P(quad), fill=255)
        return m

    # the room beyond: flipped, smaller, dimmer, with the foot of its back wall a
    # little above where the opening meets the floor, and its lamp in view
    s = FAR_SCALE
    far = ImageOps.mirror(clean).resize((round(w * s), round(h * s)), Image.LANCZOS)
    far = ImageEnhance.Brightness(far).enhance(FAR_DIM)
    ox = FAR_LAMP * w - far.width * LAMP_AT
    oy = (OPENING[3][1] + OPENING[2][1]) / 2 * h - 0.03 * h - far.height * BASE
    # above it, the top of its wall carries on up, so the opening is filled however tall
    if oy > 0:
        strip = far.crop((0, 0, far.width, round(oy) + 2)).transpose(Image.FLIP_TOP_BOTTOM)
        tall = Image.new("RGBA", (far.width, far.height + round(oy)))
        tall.paste(strip, (0, 0)); tall.paste(far, (0, round(oy)))
        far, oy = tall, 0
    beyond = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    beyond.paste(far, (round(ox), round(oy)))

    # the wall: one wide stretch of the back wall, its lamplight evened out and a
    # little shaded, laid over the wall in perspective
    px0, py0, px1, py1 = PAPER
    paper = clean.crop((round(px0 * w), round(py0 * h), round(px1 * w), round(py1 * h)))
    arr = np.asarray(paper).astype(float)
    glow = np.asarray(paper.convert("RGB").filter(ImageFilter.GaussianBlur(paper.width / 6))).astype(float)
    arr[..., :3] = np.clip(arr[..., :3] / np.maximum(glow, 1) * glow.reshape(-1, 3).mean(axis=0) * WALL_SHADE, 0, 255)
    paper = Image.fromarray(arr.astype(np.uint8))
    # laid end to end, every other one turned over, so the joins match (the pattern
    # and the panels are symmetrical)
    run = Image.new("RGBA", (paper.width * 4, paper.height))
    for i in range(4): run.paste(paper if i % 2 == 0 else ImageOps.mirror(paper), (i * paper.width, 0))
    paper = run
    pw, ph = paper.size
    wall = paper.transform((w, h), Image.PERSPECTIVE, coeffs([(0, 0), (pw, 0), (pw, ph), (0, ph)], P(WALL)), Image.BICUBIC)
    room.paste(wall, (0, 0), mask(WALL))

    # its end against the back wall and its foot, inked
    d = ImageDraw.Draw(room)
    d.line([P(WALL)[0], P(WALL)[3], P(WALL)[2]], fill=INK, width=max(2, round(w * 0.003)), joint="curve")

    # the opening, right down to the floor, and the frame round it
    room.paste(beyond, (0, 0), mask(OPENING))
    frame = keyed(FRAME) if os.path.exists(FRAME) else draw_frame()
    x0, y0, x1, y1 = inside(frame)
    fw = frame.transform((w, h), Image.PERSPECTIVE, coeffs([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], P(OPENING)), Image.BICUBIC)
    room.alpha_composite(fw)
    room.convert("RGB").save(OUT, quality=92)
    print(OUT)

if __name__ == "__main__":
    main()
