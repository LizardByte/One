#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

#include <QCoreApplication>

#include <SDL3/SDL_gamepad.h>

#include "desktop_gamepad_manager.hpp"

namespace {

bool expect_button(const SDL_GamepadButton native, const one::protocol::ControllerButton expected) {
  if (gamepadButtonForSdlButton(native) == one::protocol::controller_button(expected)) {
    return true;
  }
  std::cerr << "SDL gamepad button mapped to the wrong protocol button\n";
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  DesktopGamepadManager manager;
  if (!manager.available()) {
    std::cerr << "SDL3 gamepad subsystem is unavailable\n";
    return EXIT_FAILURE;
  }
  manager.rumble(16, 1, 1);
  manager.rumbleTriggers(16, 1, 1);
  manager.setMotionReporting(16, one::protocol::ControllerMotionType::accelerometer, 100);

  using one::protocol::ControllerButton;
  if (!expect_button(SDL_GAMEPAD_BUTTON_SOUTH, ControllerButton::a) ||
      !expect_button(SDL_GAMEPAD_BUTTON_MISC1, ControllerButton::misc) ||
      !expect_button(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, ControllerButton::paddle_1) ||
      !expect_button(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, ControllerButton::paddle_4) ||
      !expect_button(SDL_GAMEPAD_BUTTON_TOUCHPAD, ControllerButton::touchpad)) {
    return EXIT_FAILURE;
  }
  if (invertGamepadYAxis(std::numeric_limits<std::int16_t>::min()) !=
          std::numeric_limits<std::int16_t>::max() ||
      invertGamepadYAxis(std::numeric_limits<std::int16_t>::max()) !=
          -std::numeric_limits<std::int16_t>::max() ||
      gamepadTriggerValue(-1) != 0 || gamepadTriggerValue(0) != 0 ||
      gamepadTriggerValue(std::numeric_limits<std::int16_t>::max()) !=
          std::numeric_limits<std::uint8_t>::max()) {
    std::cerr << "SDL gamepad axes mapped to the wrong protocol range\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
