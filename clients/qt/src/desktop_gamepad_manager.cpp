#include "desktop_gamepad_manager.hpp"

#include <algorithm>
#include <array>
#include <limits>

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
  return {
      .analog_triggers = SDL_GamepadHasAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) ||
                         SDL_GamepadHasAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER),
      .rumble = properties != 0 &&
                SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false),
      .trigger_rumble =
          properties != 0 &&
          SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false),
  };
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
  struct ControllerSlot {
    SDL_Gamepad* gamepad {nullptr};
    SDL_JoystickID instance_id {0};
    one::protocol::ControllerState state;
  };

  explicit Impl(DesktopGamepadManager* manager) : owner(manager) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!initialized) {
      qWarning() << "Unable to initialize SDL gamepads:" << SDL_GetError();
      return;
    }
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
  }

  void attach(one::protocol::StreamingSession* new_session) noexcept {
    if (new_session == nullptr) {
      stop_feedback();
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
    }
  }

  DesktopGamepadManager* owner;
  QTimer timer;
  std::array<ControllerSlot, maximum_controllers> controller_slots;
  one::protocol::StreamingSession* session {nullptr};
  int polls_since_scan {0};
  bool initialized {false};
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
