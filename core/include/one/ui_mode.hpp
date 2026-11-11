#pragma once

#include <cstdint>
#include <string_view>

namespace one {

enum class DeviceClass : std::uint8_t {
  unknown,
  desktop,
  handheld,
  tablet,
  television,
};

enum class UiMode : std::uint8_t {
  desktop,
  touch,
  television,
};

struct DeviceTraits {
  DeviceClass device_class {DeviceClass::unknown};
  bool has_pointer {false};
  bool has_touch {false};
  bool has_directional_navigation {false};
};

[[nodiscard]] UiMode select_ui_mode(const DeviceTraits& traits) noexcept;
[[nodiscard]] std::string_view to_string(UiMode mode) noexcept;

}  // namespace one

