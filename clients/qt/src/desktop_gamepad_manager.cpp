#include "desktop_gamepad_manager.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include <QDebug>
#include <QTimer>

#ifdef ONE_HAS_PC_MEDIA
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#endif

namespace {

constexpr std::size_t maximum_controllers = 16;
constexpr std::uint32_t rumble_duration_ms = 30'000;
constexpr std::uint64_t battery_poll_interval_ms = 1'000;
constexpr float radians_to_degrees = 57.2957795F;

bool states_equal(
    const one::protocol::ControllerState& left,
    const one::protocol::ControllerState& right) noexcept {
  return left.buttons == right.buttons && left.left_trigger == right.left_trigger &&
         left.right_trigger == right.right_trigger && left.left_stick_x == right.left_stick_x &&
         left.left_stick_y == right.left_stick_y && left.right_stick_x == right.right_stick_x &&
         left.right_stick_y == right.right_stick_y;
}

#ifdef ONE_HAS_PC_MEDIA
one::protocol::ControllerType controller_type(SDL_Gamepad* gamepad) noexcept {
  switch (SDL_GetGamepadType(gamepad)) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
      return one::protocol::ControllerType::xbox;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
      return one::protocol::ControllerType::playstation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
    case SDL_GAMEPAD_TYPE_GAMECUBE:
      return one::protocol::ControllerType::nintendo;
    case SDL_GAMEPAD_TYPE_UNKNOWN:
    case SDL_GAMEPAD_TYPE_STANDARD:
    case SDL_GAMEPAD_TYPE_COUNT:
      break;
  }
  return SDL_GetGamepadVendor(gamepad) == 0x28DE ? one::protocol::ControllerType::steam
                                                 : one::protocol::ControllerType::unknown;
}

one::protocol::ControllerButtonMask supported_buttons(SDL_Gamepad* gamepad) noexcept {
  one::protocol::ControllerButtonMask supported = 0;
  for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
    const auto native_button = static_cast<SDL_GamepadButton>(button);
    if (SDL_GamepadHasButton(gamepad, native_button)) {
      supported |= gamepadButtonForSdlButton(button);
    }
  }
  return supported;
}

one::protocol::ControllerCapabilities controller_capabilities(SDL_Gamepad* gamepad) noexcept {
  const auto properties = SDL_GetGamepadProperties(gamepad);
  const auto touchpad_count = SDL_GetNumGamepadTouchpads(gamepad);
  return {
      .analog_triggers = SDL_GamepadHasAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) ||
                         SDL_GamepadHasAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER),
      .rumble = properties != 0 &&
                SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false),
      .trigger_rumble =
          properties != 0 &&
          SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false),
      .touchpad = touchpad_count > 0,
      .dual_touchpad = touchpad_count > 1,
      .accelerometer = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL),
      .gyroscope = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO),
      .battery_state = true,
  };
}

one::protocol::ControllerBattery read_battery(SDL_Gamepad* gamepad) noexcept {
  int percentage = -1;
  const auto power_state = SDL_GetGamepadPowerInfo(gamepad, &percentage);
  const auto percentage_is_valid = percentage >= 0 && percentage <= 100;
  const auto normalized_percentage = percentage_is_valid
                                       ? static_cast<std::uint8_t>(percentage)
                                       : one::protocol::ControllerBattery::unknown_percentage;
  switch (power_state) {
    case SDL_POWERSTATE_ON_BATTERY:
      return {
          .state = one::protocol::ControllerBatteryState::discharging,
          .percentage = normalized_percentage,
      };
    case SDL_POWERSTATE_NO_BATTERY:
      return {.state = one::protocol::ControllerBatteryState::not_present};
    case SDL_POWERSTATE_CHARGING:
      return {
          .state = one::protocol::ControllerBatteryState::charging,
          .percentage = normalized_percentage,
      };
    case SDL_POWERSTATE_CHARGED:
      return {
          .state = one::protocol::ControllerBatteryState::full,
          .percentage = percentage_is_valid ? normalized_percentage : static_cast<std::uint8_t>(100),
      };
    case SDL_POWERSTATE_ERROR:
    case SDL_POWERSTATE_UNKNOWN:
      return {};
  }
  return {};
}

bool batteries_equal(
    const one::protocol::ControllerBattery left,
    const one::protocol::ControllerBattery right) noexcept {
  return left.state == right.state && left.percentage == right.percentage;
}

one::protocol::ControllerState read_state(SDL_Gamepad* gamepad) noexcept {
  one::protocol::ControllerState state;
  for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
    const auto native_button = static_cast<SDL_GamepadButton>(button);
    if (SDL_GetGamepadButton(gamepad, native_button)) {
      state.buttons |= gamepadButtonForSdlButton(button);
    }
  }
  state.left_trigger = gamepadTriggerValue(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
  state.right_trigger = gamepadTriggerValue(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
  state.left_stick_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
  state.left_stick_y = invertGamepadYAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
  state.right_stick_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
  state.right_stick_y = invertGamepadYAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
  return state;
}
#endif

}  // namespace

one::protocol::ControllerButtonMask gamepadButtonForSdlButton(const int button) noexcept {
#ifdef ONE_HAS_PC_MEDIA
  using one::protocol::ControllerButton;
  switch (static_cast<SDL_GamepadButton>(button)) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
      return one::protocol::controller_button(ControllerButton::a);
    case SDL_GAMEPAD_BUTTON_EAST:
      return one::protocol::controller_button(ControllerButton::b);
    case SDL_GAMEPAD_BUTTON_WEST:
      return one::protocol::controller_button(ControllerButton::x);
    case SDL_GAMEPAD_BUTTON_NORTH:
      return one::protocol::controller_button(ControllerButton::y);
    case SDL_GAMEPAD_BUTTON_BACK:
      return one::protocol::controller_button(ControllerButton::back);
    case SDL_GAMEPAD_BUTTON_GUIDE:
      return one::protocol::controller_button(ControllerButton::guide);
    case SDL_GAMEPAD_BUTTON_START:
      return one::protocol::controller_button(ControllerButton::start);
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
      return one::protocol::controller_button(ControllerButton::left_stick);
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
      return one::protocol::controller_button(ControllerButton::right_stick);
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
      return one::protocol::controller_button(ControllerButton::left_shoulder);
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
      return one::protocol::controller_button(ControllerButton::right_shoulder);
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
      return one::protocol::controller_button(ControllerButton::dpad_up);
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
      return one::protocol::controller_button(ControllerButton::dpad_down);
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
      return one::protocol::controller_button(ControllerButton::dpad_left);
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
      return one::protocol::controller_button(ControllerButton::dpad_right);
    case SDL_GAMEPAD_BUTTON_MISC1:
      return one::protocol::controller_button(ControllerButton::misc);
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
      return one::protocol::controller_button(ControllerButton::paddle_1);
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
      return one::protocol::controller_button(ControllerButton::paddle_2);
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
      return one::protocol::controller_button(ControllerButton::paddle_3);
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
      return one::protocol::controller_button(ControllerButton::paddle_4);
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
      return one::protocol::controller_button(ControllerButton::touchpad);
    case SDL_GAMEPAD_BUTTON_INVALID:
    case SDL_GAMEPAD_BUTTON_MISC2:
    case SDL_GAMEPAD_BUTTON_MISC3:
    case SDL_GAMEPAD_BUTTON_MISC4:
    case SDL_GAMEPAD_BUTTON_MISC5:
    case SDL_GAMEPAD_BUTTON_MISC6:
    case SDL_GAMEPAD_BUTTON_COUNT:
      return 0;
  }
#else
  static_cast<void>(button);
#endif
  return 0;
}

std::int16_t invertGamepadYAxis(const std::int16_t value) noexcept {
  return static_cast<std::int16_t>(-
      std::max(value, static_cast<std::int16_t>(-std::numeric_limits<std::int16_t>::max())));
}

std::uint8_t gamepadTriggerValue(const std::int16_t value) noexcept {
  const auto non_negative = std::max<std::int32_t>(value, 0);
  return static_cast<std::uint8_t>(
      (non_negative * std::numeric_limits<std::uint8_t>::max()) /
      std::numeric_limits<std::int16_t>::max());
}

struct DesktopGamepadManager::Impl {
#ifdef ONE_HAS_PC_MEDIA
  struct TouchPoint {
    bool down {false};
    float x {0.0F};
    float y {0.0F};
    float pressure {0.0F};
  };

  struct MotionReporter {
    std::uint64_t period_ms {0};
    std::uint64_t last_report_ms {0};
    std::array<float, 3> last_sample {};
    bool has_sample {false};
  };

  struct ControllerSlot {
    SDL_Gamepad* gamepad {nullptr};
    SDL_JoystickID instance_id {0};
    one::protocol::ControllerState state;
    std::array<std::vector<TouchPoint>, 2> touchpads;
    MotionReporter accelerometer;
    MotionReporter gyroscope;
    one::protocol::ControllerBattery battery;
    std::uint64_t last_battery_poll_ms {0};
    bool battery_initialized {false};
  };

  explicit Impl(DesktopGamepadManager* manager) : owner(manager) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!initialized) {
      qWarning() << "Unable to initialize SDL gamepads:" << SDL_GetError();
      return;
    }
    gamepad_events_were_enabled = SDL_GamepadEventsEnabled();
    SDL_SetGamepadEventsEnabled(false);
    scan();
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(4);
    QObject::connect(&timer, &QTimer::timeout, owner, [this]() { poll(); });
    timer.start();
  }

  ~Impl() {
    timer.stop();
    attach(nullptr);
    for (auto& slot : controller_slots) {
      if (slot.gamepad != nullptr) {
        SDL_CloseGamepad(slot.gamepad);
        slot = {};
      }
    }
    if (initialized) {
      SDL_SetGamepadEventsEnabled(gamepad_events_were_enabled);
      SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
  }

  std::uint16_t active_mask() const noexcept {
    std::uint16_t mask = 0;
    for (std::size_t slot = 0; slot < controller_slots.size(); ++slot) {
      if (controller_slots[slot].gamepad != nullptr) {
        mask |= static_cast<std::uint16_t>(1U << slot);
      }
    }
    return mask;
  }

  int count() const noexcept {
    return static_cast<int>(std::count_if(
        controller_slots.begin(), controller_slots.end(), [](const ControllerSlot& slot) {
      return slot.gamepad != nullptr;
    }));
  }

  static void initialize_touchpads(ControllerSlot& slot) {
    const auto touchpad_count = std::clamp(SDL_GetNumGamepadTouchpads(slot.gamepad), 0, 2);
    for (int touchpad = 0; touchpad < touchpad_count; ++touchpad) {
      const auto finger_count =
          std::max(SDL_GetNumGamepadTouchpadFingers(slot.gamepad, touchpad), 0);
      slot.touchpads[static_cast<std::size_t>(touchpad)].assign(
          static_cast<std::size_t>(finger_count), {});
    }
    for (int touchpad = touchpad_count; touchpad < 2; ++touchpad) {
      slot.touchpads[static_cast<std::size_t>(touchpad)].clear();
    }
  }

  void send_battery(const std::size_t slot_number, const std::uint64_t now_ms) noexcept {
    auto& slot = controller_slots[slot_number];
    if (session == nullptr || !session->active() || slot.gamepad == nullptr) {
      return;
    }
    const auto battery = read_battery(slot.gamepad);
    slot.last_battery_poll_ms = now_ms;
    if (slot.battery_initialized && batteries_equal(slot.battery, battery)) {
      return;
    }
    slot.battery = battery;
    slot.battery_initialized = true;
    static_cast<void>(session->send_controller_battery(
        static_cast<std::uint8_t>(slot_number), battery));
  }

  void stop_telemetry() noexcept {
    for (std::size_t slot_number = 0; slot_number < controller_slots.size(); ++slot_number) {
      auto& slot = controller_slots[slot_number];
      if (slot.gamepad == nullptr) {
        continue;
      }
      for (std::size_t touchpad = 0; touchpad < slot.touchpads.size(); ++touchpad) {
        auto& points = slot.touchpads[touchpad];
        for (std::size_t pointer = 0; pointer < points.size(); ++pointer) {
          if (points[pointer].down && session != nullptr && session->active()) {
            static_cast<void>(session->send_controller_touch(
                static_cast<std::uint8_t>(slot_number),
                static_cast<std::uint8_t>(touchpad),
                static_cast<std::uint32_t>(pointer),
                one::protocol::ControllerTouchAction::cancel,
                0.0F,
                0.0F,
                0.0F));
          }
          points[pointer] = {};
        }
      }
      if (SDL_GamepadHasSensor(slot.gamepad, SDL_SENSOR_ACCEL)) {
        static_cast<void>(SDL_SetGamepadSensorEnabled(slot.gamepad, SDL_SENSOR_ACCEL, false));
      }
      if (SDL_GamepadHasSensor(slot.gamepad, SDL_SENSOR_GYRO)) {
        static_cast<void>(SDL_SetGamepadSensorEnabled(slot.gamepad, SDL_SENSOR_GYRO, false));
      }
      slot.accelerometer = {};
      slot.gyroscope = {};
      slot.battery_initialized = false;
      slot.last_battery_poll_ms = 0;
    }
  }

  void send_arrival(const std::size_t slot_number) noexcept {
    const auto& slot = controller_slots[slot_number];
    if (session == nullptr || !session->active() || slot.gamepad == nullptr) {
      return;
    }
    static_cast<void>(session->send_controller_arrival(
        static_cast<std::uint8_t>(slot_number),
        active_mask(),
        controller_type(slot.gamepad),
        supported_buttons(slot.gamepad),
        controller_capabilities(slot.gamepad)));
    static_cast<void>(
        session->send_controller_state(static_cast<std::uint8_t>(slot_number), active_mask(), slot.state));
    send_battery(slot_number, SDL_GetTicks());
  }

  void attach(one::protocol::StreamingSession* new_session) noexcept {
    if (new_session == nullptr) {
      stop_feedback();
      stop_telemetry();
    }
    session = new_session;
    if (session == nullptr || !session->active()) {
      return;
    }
    for (std::size_t slot = 0; slot < controller_slots.size(); ++slot) {
      send_arrival(slot);
    }
  }

  void rumble(
      const std::uint16_t controller_number,
      const std::uint16_t low_frequency_motor,
      const std::uint16_t high_frequency_motor) noexcept {
    if (controller_number >= controller_slots.size()) {
      return;
    }
    auto* gamepad = controller_slots[controller_number].gamepad;
    if (gamepad == nullptr || !SDL_GamepadConnected(gamepad)) {
      return;
    }
    const auto properties = SDL_GetGamepadProperties(gamepad);
    if (properties == 0 ||
        !SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)) {
      return;
    }
    if (!SDL_RumbleGamepad(
            gamepad, low_frequency_motor, high_frequency_motor, rumble_duration_ms)) {
      qWarning() << "Unable to rumble SDL gamepad:" << SDL_GetError();
    }
  }

  void rumble_triggers(
      const std::uint16_t controller_number,
      const std::uint16_t left_trigger_motor,
      const std::uint16_t right_trigger_motor) noexcept {
    if (controller_number >= controller_slots.size()) {
      return;
    }
    auto* gamepad = controller_slots[controller_number].gamepad;
    if (gamepad == nullptr || !SDL_GamepadConnected(gamepad)) {
      return;
    }
    const auto properties = SDL_GetGamepadProperties(gamepad);
    if (properties == 0 ||
        !SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false)) {
      return;
    }
    if (!SDL_RumbleGamepadTriggers(
            gamepad, left_trigger_motor, right_trigger_motor, rumble_duration_ms)) {
      qWarning() << "Unable to rumble SDL gamepad triggers:" << SDL_GetError();
    }
  }

  void set_motion_reporting(
      const std::uint16_t controller_number,
      const one::protocol::ControllerMotionType type,
      const std::uint16_t report_rate_hz) noexcept {
    if (controller_number >= controller_slots.size()) {
      return;
    }
    auto& slot = controller_slots[controller_number];
    if (slot.gamepad == nullptr || !SDL_GamepadConnected(slot.gamepad)) {
      return;
    }
    const auto sensor_type = type == one::protocol::ControllerMotionType::accelerometer
                               ? SDL_SENSOR_ACCEL
                               : SDL_SENSOR_GYRO;
    auto& reporter = type == one::protocol::ControllerMotionType::accelerometer
                       ? slot.accelerometer
                       : slot.gyroscope;
    if (!SDL_GamepadHasSensor(slot.gamepad, sensor_type)) {
      reporter = {};
      return;
    }
    if (report_rate_hz == 0) {
      static_cast<void>(SDL_SetGamepadSensorEnabled(slot.gamepad, sensor_type, false));
      reporter = {};
      return;
    }
    if (!SDL_SetGamepadSensorEnabled(slot.gamepad, sensor_type, true)) {
      qWarning() << "Unable to enable SDL gamepad sensor:" << SDL_GetError();
      reporter = {};
      return;
    }
    reporter = {
        .period_ms = std::max<std::uint64_t>(1, 1'000 / report_rate_hz),
    };
  }

  void poll_touchpads(const std::size_t slot_number) noexcept {
    auto& slot = controller_slots[slot_number];
    const auto touchpad_count = std::clamp(SDL_GetNumGamepadTouchpads(slot.gamepad), 0, 2);
    for (int touchpad = 0; touchpad < touchpad_count; ++touchpad) {
      const auto finger_count =
          std::max(SDL_GetNumGamepadTouchpadFingers(slot.gamepad, touchpad), 0);
      auto& points = slot.touchpads[static_cast<std::size_t>(touchpad)];
      if (points.size() != static_cast<std::size_t>(finger_count)) {
        points.assign(static_cast<std::size_t>(finger_count), {});
      }
      for (int finger = 0; finger < finger_count; ++finger) {
        bool down = false;
        float x = 0.0F;
        float y = 0.0F;
        float pressure = 0.0F;
        if (!SDL_GetGamepadTouchpadFinger(
                slot.gamepad, touchpad, finger, &down, &x, &y, &pressure)) {
          continue;
        }
        auto& prior = points[static_cast<std::size_t>(finger)];
        const auto changed = prior.x != x || prior.y != y || prior.pressure != pressure;
        if (!prior.down && !down) {
          prior = {.down = false, .x = x, .y = y, .pressure = pressure};
          continue;
        }
        const auto action = !prior.down && down
                              ? one::protocol::ControllerTouchAction::down
                              : (prior.down && !down
                                     ? one::protocol::ControllerTouchAction::up
                                     : one::protocol::ControllerTouchAction::move);
        if (action != one::protocol::ControllerTouchAction::move || changed) {
          static_cast<void>(session->send_controller_touch(
              static_cast<std::uint8_t>(slot_number),
              static_cast<std::uint8_t>(touchpad),
              static_cast<std::uint32_t>(finger),
              action,
              std::clamp(x, 0.0F, 1.0F),
              std::clamp(y, 0.0F, 1.0F),
              std::clamp(pressure, 0.0F, 1.0F)));
        }
        prior = {.down = down, .x = x, .y = y, .pressure = pressure};
      }
    }
  }

  void poll_motion_sensor(
      const std::size_t slot_number,
      const one::protocol::ControllerMotionType type,
      const SDL_SensorType sensor_type,
      MotionReporter& reporter,
      const std::uint64_t now_ms) noexcept {
    if (reporter.period_ms == 0 || now_ms - reporter.last_report_ms < reporter.period_ms) {
      return;
    }
    std::array<float, 3> sample {};
    auto& slot = controller_slots[slot_number];
    if (!SDL_GetGamepadSensorData(slot.gamepad, sensor_type, sample.data(), sample.size())) {
      return;
    }
    reporter.last_report_ms = now_ms;
    if (reporter.has_sample && reporter.last_sample == sample) {
      return;
    }
    reporter.last_sample = sample;
    reporter.has_sample = true;
    if (type == one::protocol::ControllerMotionType::gyroscope) {
      for (auto& axis : sample) {
        axis *= radians_to_degrees;
      }
    }
    static_cast<void>(session->send_controller_motion(
        static_cast<std::uint8_t>(slot_number), type, sample[0], sample[1], sample[2]));
  }

  void stop_feedback() noexcept {
    for (auto& slot : controller_slots) {
      if (slot.gamepad == nullptr || !SDL_GamepadConnected(slot.gamepad)) {
        continue;
      }
      const auto properties = SDL_GetGamepadProperties(slot.gamepad);
      if (properties == 0) {
        continue;
      }
      if (SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)) {
        static_cast<void>(SDL_RumbleGamepad(slot.gamepad, 0, 0, 0));
      }
      if (SDL_GetBooleanProperty(
              properties, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false)) {
        static_cast<void>(SDL_RumbleGamepadTriggers(slot.gamepad, 0, 0, 0));
      }
    }
  }

  void remove_disconnected() noexcept {
    bool changed = false;
    for (std::size_t slot_number = 0; slot_number < controller_slots.size(); ++slot_number) {
      auto& slot = controller_slots[slot_number];
      if (slot.gamepad == nullptr || SDL_GamepadConnected(slot.gamepad)) {
        continue;
      }
      SDL_CloseGamepad(slot.gamepad);
      slot = {};
      changed = true;
      if (session != nullptr && session->active()) {
        static_cast<void>(
            session->send_controller_departure(static_cast<std::uint8_t>(slot_number), active_mask()));
      }
    }
    if (changed) {
      emit owner->controllerCountChanged();
    }
  }

  void scan() noexcept {
    remove_disconnected();
    int gamepad_count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
    if (gamepads == nullptr) {
      return;
    }
    bool changed = false;
    for (int gamepad_index = 0; gamepad_index < gamepad_count; ++gamepad_index) {
      const auto instance_id = gamepads[gamepad_index];
      const auto already_open = std::any_of(
          controller_slots.begin(),
          controller_slots.end(),
          [instance_id](const ControllerSlot& slot) {
            return slot.gamepad != nullptr && slot.instance_id == instance_id;
          });
      if (already_open) {
        continue;
      }
      const auto available_slot = std::find_if(
          controller_slots.begin(), controller_slots.end(), [](const ControllerSlot& slot) {
            return slot.gamepad == nullptr;
          });
      if (available_slot == controller_slots.end()) {
        break;
      }
      auto* gamepad = SDL_OpenGamepad(instance_id);
      if (gamepad == nullptr) {
        qWarning() << "Unable to open SDL gamepad:" << SDL_GetError();
        continue;
      }
      const auto slot_number =
          static_cast<std::size_t>(std::distance(controller_slots.begin(), available_slot));
      *available_slot = {
          .gamepad = gamepad,
          .instance_id = instance_id,
          .state = read_state(gamepad),
      };
      initialize_touchpads(*available_slot);
      SDL_SetGamepadPlayerIndex(gamepad, static_cast<int>(slot_number));
      changed = true;
      send_arrival(slot_number);
    }
    SDL_free(gamepads);
    if (changed) {
      emit owner->controllerCountChanged();
    }
  }

  void poll() noexcept {
    SDL_PumpEvents();
    SDL_UpdateGamepads();
    if (++polls_since_scan >= 250) {
      polls_since_scan = 0;
      scan();
    }
    if (session == nullptr || !session->active()) {
      return;
    }
    const auto now_ms = SDL_GetTicks();
    const auto mask = active_mask();
    for (std::size_t slot_number = 0; slot_number < controller_slots.size(); ++slot_number) {
      auto& slot = controller_slots[slot_number];
      if (slot.gamepad == nullptr || !SDL_GamepadConnected(slot.gamepad)) {
        continue;
      }
      const auto current_state = read_state(slot.gamepad);
      if (!states_equal(current_state, slot.state)) {
        slot.state = current_state;
        static_cast<void>(
            session->send_controller_state(static_cast<std::uint8_t>(slot_number), mask, current_state));
      }
      poll_touchpads(slot_number);
      poll_motion_sensor(
          slot_number,
          one::protocol::ControllerMotionType::accelerometer,
          SDL_SENSOR_ACCEL,
          slot.accelerometer,
          now_ms);
      poll_motion_sensor(
          slot_number,
          one::protocol::ControllerMotionType::gyroscope,
          SDL_SENSOR_GYRO,
          slot.gyroscope,
          now_ms);
      if (!slot.battery_initialized ||
          now_ms - slot.last_battery_poll_ms >= battery_poll_interval_ms) {
        send_battery(slot_number, now_ms);
      }
    }
  }

  DesktopGamepadManager* owner;
  QTimer timer;
  std::array<ControllerSlot, maximum_controllers> controller_slots;
  one::protocol::StreamingSession* session {nullptr};
  int polls_since_scan {0};
  bool initialized {false};
  bool gamepad_events_were_enabled {true};
#else
  explicit Impl(DesktopGamepadManager*) {}
#endif
};

DesktopGamepadManager::DesktopGamepadManager(QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>(this)) {}

DesktopGamepadManager::~DesktopGamepadManager() = default;

bool DesktopGamepadManager::available() const noexcept {
#ifdef ONE_HAS_PC_MEDIA
  return impl_->initialized;
#else
  return false;
#endif
}

int DesktopGamepadManager::controllerCount() const noexcept {
#ifdef ONE_HAS_PC_MEDIA
  return impl_->count();
#else
  return 0;
#endif
}

std::uint16_t DesktopGamepadManager::activeMask() const noexcept {
#ifdef ONE_HAS_PC_MEDIA
  return impl_->active_mask();
#else
  return 0;
#endif
}

void DesktopGamepadManager::attachSession(one::protocol::StreamingSession* session) noexcept {
#ifdef ONE_HAS_PC_MEDIA
  impl_->attach(session);
#else
  static_cast<void>(session);
#endif
}

void DesktopGamepadManager::rumble(
    const std::uint16_t controller_number,
    const std::uint16_t low_frequency_motor,
    const std::uint16_t high_frequency_motor) noexcept {
#ifdef ONE_HAS_PC_MEDIA
  impl_->rumble(controller_number, low_frequency_motor, high_frequency_motor);
#else
  static_cast<void>(controller_number);
  static_cast<void>(low_frequency_motor);
  static_cast<void>(high_frequency_motor);
#endif
}

void DesktopGamepadManager::rumbleTriggers(
    const std::uint16_t controller_number,
    const std::uint16_t left_trigger_motor,
    const std::uint16_t right_trigger_motor) noexcept {
#ifdef ONE_HAS_PC_MEDIA
  impl_->rumble_triggers(controller_number, left_trigger_motor, right_trigger_motor);
#else
  static_cast<void>(controller_number);
  static_cast<void>(left_trigger_motor);
  static_cast<void>(right_trigger_motor);
#endif
}

void DesktopGamepadManager::setMotionReporting(
    const std::uint16_t controller_number,
    const one::protocol::ControllerMotionType type,
    const std::uint16_t report_rate_hz) noexcept {
#ifdef ONE_HAS_PC_MEDIA
  impl_->set_motion_reporting(controller_number, type, report_rate_hz);
#else
  static_cast<void>(controller_number);
  static_cast<void>(type);
  static_cast<void>(report_rate_hz);
#endif
}
