<div align="center">
  <img
    src="https://raw.githubusercontent.com/LizardByte/.github/refs/heads/master/branding/logos/logo.svg"
    alt="LizardByte icon"
    width="256"
  />
  <h1 align="center">One</h1>
  <h4 align="center">A universal game-streaming client for Sunshine.</h4>
</div>

<div align="center">
  <a href="https://github.com/SunshineStream/One"><img src="https://img.shields.io/github/stars/SunshineStream/One.svg?logo=github&style=for-the-badge" alt="GitHub stars"></a>
</div>

One is a [SunshineStream](https://github.com/SunshineStream) project under the
[LizardByte](https://app.lizardbyte.dev/) umbrella.

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
- An initial PC stream path with FFmpeg software video decoding, Opus/SDL3 audio output, captured keyboard and mouse
  input, and SDL3 controller input with rumble, touchpad, motion, and battery reporting. It currently negotiates H.264
  SDR while hardware decoding and platform-specific controller refinements are still being built.
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

Install Qt 6.8 or later with Qt Quick and Qt Quick Controls, plus the SDL3 development package.
Desktop builds use the `third-party/build-deps` submodule, just like Sunshine. CMake selects the
release tag on that submodule's current commit and downloads the matching platform's FFmpeg archive:

```shell
cmake -S . -B build/qt -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build/qt
```

Launch the desktop build directly from its build directory (for example, `./build/qt/one` on Unix-like systems).
Windows builds default to fully static third-party dependencies. With the MSYS2 UCRT64 toolchain, install
`mingw-w64-ucrt-x86_64-qt6-static` and SDL3, then build from PowerShell:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake --preset windows-static
cmake --build --preset windows-static
ctest --preset windows-static
.\build\windows-static\one.exe
```

Open a paired computer, choose an application, and select **Stream**. Click the video to capture the keyboard and
mouse; press **Ctrl+Alt+Shift+Z** to release them. Controllers connected through SDL3 are detected before launch and
can be connected or removed while streaming. Standard and trigger rumble are enabled when SDL3 reports support.
Touchpad contacts, host-requested motion sensors, and battery state are reported when SDL3 exposes them. Adaptive
trigger effects are supported on SDL3 DualSense devices, and RGB LED updates are enabled when SDL3 reports support.

Multiple Sunshine instances on one computer are listed separately, with their HTTP ports shown beside the address.
To add one manually, enter `hostname:port`, `192.168.1.100:48089`, or `[::1]:48089` using that instance's HTTP port
(default `47989`); One uses its advertised HTTPS port for authenticated requests. Pairing trust is scoped to the
server UUID and HTTP port. A certificate mismatch or authenticated authorization rejection marks only that instance
**Unpaired**, clears its cached applications, and requires PIN pairing again. Temporary connection failures do not
revoke pairing. Existing default-port pairings are migrated; custom-port instances must be paired independently.

The Windows executable can run by itself, without MSYS2 on `PATH` or a Qt plugin directory. Qt/QML plugins,
OpenSSL, SDL3, Opus, dav1d, FFmpeg, and the MinGW runtimes are statically linked; only Windows operating-system DLLs
are imported. The build checks the executable's import table and fails if a third-party DLL slips in.

The build-deps media package supplies static FFmpeg, dav1d, and Opus libraries, with encoders, decoders,
formats, filters, and command-line tools. One links the libraries it needs for video and audio decoding.
For an offline build, extract a matching `<platform>-ffmpeg.tar.gz` release asset and set
`FFMPEG_PREPARED_BINARIES` to its `ffmpeg` directory. To select a different published release, check out
that release tag in `third-party/build-deps`; the submodule commit is the dependency pin.
Current streaming negotiation remains H.264 SDR.

`ONE_WINDOWS_STATIC` defaults to `ON` on Windows and currently uses the MSYS2 MinGW toolchain. Static Qt is
discovered beside the compiler; set `CMAKE_PREFIX_PATH` if it is installed elsewhere. Use a fresh build directory
when switching from shared Qt. Qt upgrades also change the compile commands so all Qt-dependent objects rebuild
together even when package files retain older timestamps. Set `ONE_WINDOWS_STATIC=OFF` only for an explicitly
shared development build; that build requires its dependency DLLs on `PATH`. On other platforms,
`ONE_PREFER_STATIC_DEPENDENCIES` continues to prefer usable static libraries.

Use `--ui-mode=desktop`, `--ui-mode=touch`, or `--ui-mode=television` to preview responsive modes on a desktop
build.

The shared interface uses a compact desktop sidebar, touch navigation along the bottom, and a TV navigation bar.
The library loads application artwork from the selected Sunshine instance over its paired TLS connection; missing
artwork uses a local fallback and does not prevent streaming. Failed cover requests expose a reason and a per-title
retry action. The library uses responsive 3:4 portrait posters and a selected-title preview on larger screens;
backdrops, descriptions, and other future Sunshine metadata are not fabricated from the current application list.

Known computers (including manually added and unpaired instances) are saved between launches. Reachability is checked
at startup, every 3 seconds, and with Refresh. Saved certificates are verified with authenticated `/serverinfo`
requests, so removed or disabled clients become unpaired without opening the library. Re-enabling a still-trusted
client restores its paired state. Temporary network failures do not revoke pairing.
Offline computers stay visible with their saved pairing state;
different HTTP ports remain separate Sunshine instances.

The computer picker uses a centered, circular profile carousel inspired by
[Moonlight Xbox's UI V2 proposal](https://github.com/TheElixZammuto/moonlight-xbox/pull/283), implemented in One's shared
QML interface with lunar artwork. Paired online profiles are full moons; unpaired online profiles are half moons.
Both use a white selection ring in dark mode. Offline profiles are crescent moons regardless of pairing, with evenly
spaced gray dashed rings. Light mode uses darker rings for contrast.
Names, endpoints, and status remain visible. Arrow keys,
Home/End, swipe, and pointer selection work across layouts. Offline profiles remain selectable but cannot connect.
Navigation stays in the same place across pages: a desktop sidebar, TV top bar, or mobile/compact bottom bar.
The ambient background is static, without continuously running visual effects.
Choose **Settings → Appearance** for dark, light, or system colors; the preference is saved on the device. Stream
quality settings are currently read-only while that configuration path is being implemented.

When building tests, install Qt Test as well. The `one.qt.ui_interactions` test exercises the shared screens with
isolated sample hosts and applications, including pairing dialogs, library actions, keyboard navigation, and saved
appearance preferences. It does not connect to Sunshine. To render review images with the same fixtures:

```shell
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/qt/one_ui_test --screenshots=/path/to/screenshots
```

The `one.qt.authenticated_client` test runs two isolated loopback TLS servers to check instance isolation,
certificate validation, artwork loading, authorization failures, and library recovery without changing real Sunshine
pairings. The pairing test also exercises the asynchronous worker against a local test server.

Platform packaging and native streaming backends will be added as each existing client is brought into the shared
architecture. Sunshine and One consume the same FFmpeg package from LizardByte/build-deps.

## License

One is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`). Imported upstream code will
retain its copyright and attribution.
