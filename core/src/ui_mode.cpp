#include "one/ui_mode.hpp"

namespace one {

UiMode select_ui_mode(const DeviceTraits& traits) noexcept {
  if (traits.device_class == DeviceClass::television) {
    return UiMode::television;
  }

  if (traits.device_class == DeviceClass::handheld || traits.device_class == DeviceClass::tablet) {
    return UiMode::touch;
  }

  if (traits.has_touch && !traits.has_pointer) {
    return UiMode::touch;
  }

  if (traits.has_directional_navigation && !traits.has_pointer && !traits.has_touch) {
    return UiMode::television;
  }

  return UiMode::desktop;
}

std::string_view to_string(const UiMode mode) noexcept {
  switch (mode) {
    case UiMode::desktop:
      return "desktop";
    case UiMode::touch:
      return "touch";
    case UiMode::television:
      return "television";
  }

  return "desktop";
}

}  // namespace one

