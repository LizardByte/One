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

- A Qt-free C++20 core with host, pairing-session, and capability-driven UI models.
- A pinned `moonlight-common-c` streaming-protocol submodule with a One-owned C++ boundary.
- A pinned `qmdnsengine` submodule providing real `_nvstream._tcp` host discovery.
- A shared Qt Quick shell for desktop, Android, Android TV, iOS, and Apple TV.
- Persistent client identity, `/serverinfo` queries, the GameStream certificate pairing exchange, and authenticated
  application-list browsing in the Qt client.
- Exact authenticated `/launch`, `/resume`, and `/cancel` request handling plus a renderer-neutral
  `moonlight-common-c` session lifecycle boundary.
- An initial PC stream path with FFmpeg software video decoding, Opus/SDL audio output, and captured keyboard and
  mouse input. It currently negotiates H.264 SDR while hardware decoding and controller input are still being built.
- Android TV detection and a tvOS compile-time path.
- A placeholder boundary for the separate Xbox UWP XAML interface.
- Recorded upstream revisions for each source client that has not yet been brought behind One-owned interfaces.

Android TV and Apple TV are required targets. See [platform targets](docs/platforms.md) and the
[architecture](docs/architecture.md) for the current boundaries and validation gates.

Initialize the protocol and its nested dependencies after cloning:

```shell
git submodule update --init --recursive
```

## Build the portable core

The core does not require Qt. Its streaming protocol currently requires OpenSSL by default; configure with
`-DONE_PROTOCOL_USE_MBEDTLS=ON` for upstream's MbedTLS path.

```shell
cmake --preset core
cmake --build --preset core
ctest --preset core
```

## Build the shared Qt client

Install Qt 6.8 or later with Qt Quick and Qt Quick Controls. Desktop streaming also requires a decoder-enabled
FFmpeg, Opus, and SDL2 development package. Then configure with the dependency prefix:

```shell
cmake -S . -B build/qt -DCMAKE_PREFIX_PATH=/path/to/Qt -DONE_BUILD_QT_CLIENT=ON
cmake --build build/qt
```

Launch the desktop build directly from its build directory (for example, `./build/qt/one` on Unix-like systems).
When using the shared MSYS2 UCRT64 Qt and FFmpeg packages on Windows, include their DLL directory in `PATH`:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
.\build\qt\one.exe
```

Open a paired computer, choose an application, and select **Stream**. Click the video to capture the keyboard and
mouse; press **Ctrl+Alt+Shift+Z** to release them. Controller input is not implemented yet.

`ONE_PREFER_STATIC_DEPENDENCIES` defaults to `ON`. One, `moonlight-common-c`, qmdnsengine, OpenSSL, Opus, SDL2, and
the MinGW runtime are linked statically when the toolchain provides usable static variants. Qt remains dynamic when
configuring against a shared Qt installation. The general-purpose MSYS2 FFmpeg static closure is not usable here;
One temporarily links its FFmpeg ABI dynamically until the project has a purpose-built, decoder-only FFmpeg package.
Use a static Qt toolchain when a fully static Qt build is supported by the target platform.

Use `--ui-mode=desktop`, `--ui-mode=touch`, or `--ui-mode=television` to preview responsive modes on a desktop
build.

Platform packaging and native streaming backends will be added as each existing client is brought into the shared
architecture. One does not reuse Sunshine's encoder-focused FFmpeg build.

## License

One is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`). Imported upstream code will
retain its copyright and attribution.
