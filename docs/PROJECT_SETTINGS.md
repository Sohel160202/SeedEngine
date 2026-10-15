# Project Settings

Seed stores beginner-facing project settings in the `.seedproject` descriptor.

## Runtime settings

The runtime reads these values when launched with `--project=...`:

- `name`: window title and runtime project name.
- `window_width` and `window_height`: startup window size.
- `window_resizable`: whether the player can resize the window.
- `vsync`: enable or disable vertical synchronization.

Existing project files remain valid. If a setting is absent, Seed uses safe defaults: 1280×720, resizable, and VSync enabled.

## Project metadata

The descriptor also reserves beginner-friendly settings for the next editor workflow:

- `window_icon`: project icon path.
- `input_profile`: named input mapping profile.
- `build_profile`: named export profile such as `Development` or `Release`.

These fields are persisted now so the editor and exporter can adopt them without changing the project format again.
