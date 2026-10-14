Name:           mirror-scene
Version:        %{version}
Release:        1%{?dist}
Summary:        The mirror scene, a cartoon screensaver
License:        MIT
Source0:        mirror-scene-%{version}.tar.gz
BuildRequires:  gcc, make, pkgconfig(sdl2)
Requires:       swayidle

%description
A cartoon homage to the mirror gag of 1930s screen comedy: a man in a
nightshirt and nightcap creeps up to a mirror whose glass is gone, and the
man on the other side copies his every move, almost. Dances, ducks, hat
swaps and a dropped hat, until a third man in the same disguise walks
through the frame.

On Wayland desktops it starts by itself after five minutes idle.
mirror-scene-screensaver changes the delay or turns it off.

%prep
%setup -q

%build
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{?__global_ldflags}"

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix} SYSCONFDIR=%{_sysconfdir}
rm -rf %{buildroot}%{_datadir}/doc/mirror-scene

%files
%license LICENSE
%doc README.md
%{_bindir}/mirror-scene
%{_bindir}/mirror-scene-screensaver
%{_datadir}/applications/mirror-scene.desktop
%dir %{_datadir}/screensaver-service
%dir %{_datadir}/screensaver-service/savers
%{_datadir}/screensaver-service/savers/mirror-scene.desktop
%{_datadir}/icons/hicolor/256x256/apps/mirror-scene.png
%config(noreplace) %{_sysconfdir}/xdg/autostart/mirror-scene-screensaver.desktop
