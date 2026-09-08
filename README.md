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

Install Qt 6.8 or later with Qt Quick and Qt Quick Controls. Desktop streaming also requires a decoder-enabled
FFmpeg, Opus, and SDL3 development package. Then configure with the dependency prefix:

```shell
cmake -S . -B build/qt -DCMAKE_PREFIX_PATH=/path/to/Qt -DONE_BUILD_QT_CLIENT=ON
cmake --build build/qt
```

Launch the desktop build directly from its build directory (for example, `./build/qt/one` on Unix-like systems).
Windows builds default to fully static third-party dependencies. With the MSYS2 UCRT64 toolchain, install
`mingw-w64-ucrt-x86_64-qt6-static`, SDL3, Opus, dav1d, NASM, and the MSYS2 `make` package, then build from PowerShell:

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

The first Windows build downloads a hash-pinned FFmpeg source release and builds a decoder-only static package
inside the build directory: H.264, HEVC, and AV1 decoding, with no encoders or network stack. Current streaming
negotiation remains H.264 SDR. This is separate from Sunshine's encoder-focused package.

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
artwork uses a local fallback and does not prevent streaming.
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
architecture. One does not reuse Sunshine's encoder-focused FFmpeg build.

## License

One is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`). Imported upstream code will
retain its copyright and attribution.
