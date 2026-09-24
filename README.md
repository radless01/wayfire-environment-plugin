# wayfire-environment-plugin
A wayfire plugin, which adds [environment] section into ~/.config/wayfire.ini KEY = VALUE, written in C++.

# Installation & (Re)Build
rm -rf build
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
