#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <Qt>

#include "one/protocol/streaming_session.hpp"

struct QtKeyMapping {
  std::uint16_t virtual_key {0};
  bool normalized {true};
};

[[nodiscard]] std::optional<QtKeyMapping> mapQtKey(
    int key,
    Qt::KeyboardModifiers modifiers,
    quint32 nativeVirtualKey,
    quint32 nativeScanCode) noexcept;

[[nodiscard]] one::protocol::KeyModifiers keyModifiersForPressedKeys(
    std::span<const std::uint16_t> pressedKeys) noexcept;

[[nodiscard]] bool isReleaseCaptureShortcut(
    std::uint16_t virtualKey,
    one::protocol::KeyModifiers modifiers) noexcept;
