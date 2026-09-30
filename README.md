# wayfire-environment-plugin

A Wayfire plugin that reads variables from an `[environment]` section in your Wayfire config and exports them into the user environment.

This lets you define environment variables such as `PATH`, `XDG_CURRENT_DESKTOP`, or application-specific settings in the same configuration file used by Wayfire, without needing to maintain a separate shell profile.

## Features

- Reads variables from `[environment]` in Wayfire config files.
- Supports config discovery from the Wayfire `--config` option or the default `~/.config/wayfire.ini` / `~/.config/wayfire/wayfire.ini` locations.
- Expands shell-style variable references such as `$HOME` or `$VAR_NAME` inside values.
- Ensures the plugin is loaded early enough to apply environment variables before other plugins start.
- Imports values into the systemd user environment using `systemctl --user import-environment` and `dbus-update-activation-environment`.

## Requirements

- Wayfire >= 0.11
- Meson
- C++17 compiler
- pkg-config
- systemd (for user environment import support)

## Building

```bash
meson setup build
meson compile -C build
sudo meson install -C build
```

## Usage

Add the plugin to your Wayfire configuration and define an `[environment]` section.

Example:

```ini
[core]
plugins = environment switcher blur simple-tile

[environment]
EDITOR=nvim
BROWSER=firefox
XDG_CURRENT_DESKTOP=wayfire
MY_APP_HOME=$HOME/.local/share/my-app
GREETING="hello world"
```

The plugin will:

- locate the active Wayfire config file,
- find the `plugins` entry under `[core]`,
- make sure `environment` is loaded first, and
- apply the variables from `[environment]` to the current session.

## Example: environment variables in Wayfire

This is useful for setting runtime environment values for applications launched by Wayfire or for custom scripts and plugins that rely on a consistent user environment.

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.
