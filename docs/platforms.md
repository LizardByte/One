# Platform targets

| Target | Required | Interface | Initial bootstrap status |
| --- | --- | --- | --- |
| Windows | Yes | Shared Qt Quick | Initial H.264/Opus stream plus keyboard and mouse input added |
| Linux | Yes | Shared Qt Quick | Desktop media backend added; runtime validation pending |
| macOS | Yes | Shared Qt Quick | Desktop media backend added; runtime validation pending |
| Android mobile/tablet | Yes | Shared Qt Quick | Touch mode added |
| Android TV | Yes | Shared Qt Quick | `UiModeManager` television detection added |
| iPhone/iPad | Yes | Shared Qt Quick | Touch compile-time path added |
| Apple TV | Yes | Shared Qt Quick | tvOS compile-time path added; Qt toolchain validation pending |
| Xbox One/Series | Yes | UWP XAML | Core consumer placeholder added |

## Apple TV gate

Apple TV is not allowed to silently fall out of the shared-UI work. Before importing or rewriting substantial client
behavior, a signed-device spike must prove:

1. A Qt Quick application builds and launches on current tvOS hardware.
2. Directional focus, the remote, controllers, text entry, suspend, and resume work.
3. A VideoToolbox-backed native streaming surface composes with QML without an extra decoded-frame copy.
4. Frame pacing, HDR, and in-stream overlays do not regress compared with the imported client baseline.
5. The package passes App Store validation.

If maintaining the required Qt tvOS support proves impractical, the fallback is a native tvOS interface consuming the
same core/view model. Apple TV itself is not optional.
