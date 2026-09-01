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
- Exact authenticated `/launch` and `/resume` request handling plus a renderer-neutral `moonlight-common-c` session
  lifecycle boundary. The UI remains launch-gated until a platform video decoder is connected.
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

Install Qt 6.8 or later with Qt Quick and Qt Quick Controls, then configure with its prefix:

```shell
cmake -S . -B build/qt -DCMAKE_PREFIX_PATH=/path/to/Qt -DONE_BUILD_QT_CLIENT=ON
cmake --build build/qt
```

Launch the desktop build directly from its build directory (for example, `./build/qt/one` on Unix-like systems or
`.\build\qt\one.exe` on Windows).

`ONE_PREFER_STATIC_DEPENDENCIES` defaults to `ON`. One, `moonlight-common-c`, qmdnsengine, and OpenSSL are linked
statically when the toolchain provides static variants. Qt remains dynamic when configuring against a shared Qt
installation; use a static Qt toolchain when a fully static Qt build is supported by the target platform.

Use `--ui-mode=desktop`, `--ui-mode=touch`, or `--ui-mode=television` to preview responsive modes on a desktop
build.

Platform packaging and native streaming backends will be added as each existing client is brought into the shared
architecture. PC video decoding requires a dedicated decoder-enabled FFmpeg build; One does not reuse Sunshine's
encoder-focused FFmpeg build.

## License

One is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`). Imported upstream code will
retain its copyright and attribution.
