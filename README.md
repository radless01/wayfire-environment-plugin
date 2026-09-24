# wayfire-environment-plugin
A wayfire plugin, which adds [environment] section into ~/.config/wayfire.ini KEY = VALUE, written in C++.

## Installation & (Re)Build
rm -rf build </br>
meson setup build --prefix=/usr </br>
meson compile -C build </br>
sudo meson install -C build </br>
