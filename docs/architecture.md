# Architecture

One uses one portable streaming/application core and maximizes shared interface code without forcing platform media
and input APIs into a lowest-common-denominator abstraction.

## Runtime layers

1. `core/protocol` contains a pinned `moonlight-common-c` submodule behind a One-owned C++ interface.
2. `core` contains Qt-free C++ application behavior and data models, including host identity and pairing state.
3. `clients/qt` contains the shared Qt Quick interface for desktop, Android, Android TV, iOS, and Apple TV.
4. Platform adapters provide hardware decoding, rendering, audio, input, secure storage, and lifecycle integration.
5. `clients/xbox` provides the UWP XAML interface for Xbox while consuming the same core contracts.

The core must not expose Qt, JNI, Foundation, or WinRT types. Boundaries use portable C++ internally and a narrow C
ABI where a platform language requires one.

Discovery and pairing do not belong to `moonlight-common-c`. Platform discovery adapters feed a shared `HostCatalog`.
The Qt client uses qmdnsengine for `_nvstream._tcp` discovery and owns its GameStream HTTP, certificate, and pairing
exchange. Client identity and pinned host certificates are persistent; platform secure-storage adapters remain a
later hardening step.

## Shared interface

The Qt interface has three responsive modes selected from device capabilities:

- `desktop`: pointer-first navigation and a persistent side rail.
- `touch`: touch-first layouts with bottom navigation.
- `television`: directional navigation, larger typography, and top navigation.

Android determines television mode through `UiModeManager`. tvOS selects television mode at compile time. All builds
can preview another mode with `--ui-mode=desktop|touch|television` or the `ONE_UI_MODE` environment variable.

The final streaming surface remains platform-native. QML hosts the surface and draws menus or diagnostics over it
without copying decoded frames through the UI layer. The initial desktop bring-up backend deliberately uses FFmpeg
software decoding and a copied `QImage`; it is a protocol-integration path, not the final zero-copy renderer.

`core/protocol` owns the stream configuration, per-session cryptographic input key, launch parameters, and
`moonlight-common-c` lifecycle. Platform media adapters receive One-owned encoded-video and Opus views rather than
vendor structs. Because `moonlight-common-c` currently stores connection state globally, the boundary enforces one
active stream per process. HTTP `/launch` or `/resume` must not be sent until compatible audio and video adapters are
ready to start the protocol session immediately.

Sunshine's FFmpeg package is encoder-focused and is not a client dependency. One's eventual bundled FFmpeg build
must be decoder-only and contain the codecs needed by the negotiated client formats.

## Required design constraints

- Android TV and Apple TV are release requirements, not optional future targets.
- Platform behavior is capability-driven. Do not assume every controller or output feature exists on every device of
  an operating-system family.
- A shared UI does not require identical density or navigation on pointer, touch, and television devices.
- Xbox may render different XAML controls, but it must consume the same settings, labels, state transitions, and
  behavioral tests as the Qt application.
