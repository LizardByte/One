<div align="center">
  <img
    src="https://raw.githubusercontent.com/LizardByte/.github/refs/heads/master/branding/logos/logo.svg"
    alt="LizardByte icon"
    width="256"
  />
  <h1 align="center">One</h1>
  <h4 align="center">A universal game-streaming client for Sunshine.</h4>
</div>

One brings the desktop, Android, Android TV, iPhone, iPad, Apple TV, and Xbox clients into one repository with one
portable core and a standardized interface.

## Status

One is in its initial bootstrap phase. The repository currently contains:

- A Qt-free C++20 core with capability-driven desktop, touch, and television UI modes.
- A shared Qt Quick shell for desktop, Android, Android TV, iOS, and Apple TV.
- Android TV detection and a tvOS compile-time path.
- A placeholder boundary for the separate Xbox UWP XAML interface.
- Recorded upstream revisions; no Moonlight source has been imported yet.

Android TV and Apple TV are required targets. See [platform targets](docs/platforms.md) and the
[architecture](docs/architecture.md) for the current boundaries and validation gates.

## Build the portable core

The core does not require Qt:

```shell
cmake --preset core
cmake --build --preset core
ctest --preset core
```

## Build the shared Qt client

Install Qt 6.8 or later with Qt Quick and Qt Quick Controls, then configure with its prefix:

```shell
cmake -S . -B build/qt -DCMAKE_PREFIX_PATH=/path/to/Qt -DONE_BUILD_QT_CLIENT=ON
cmake --build build/qt
```

Use `--ui-mode=desktop`, `--ui-mode=touch`, or `--ui-mode=television` to preview responsive modes on a desktop
build.

Platform packaging and native streaming backends will be added as each existing client is brought into the shared
architecture.

## License

One is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`). Imported upstream code will
retain its copyright and attribution.
