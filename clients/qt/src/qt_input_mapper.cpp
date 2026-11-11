#include "qt_input_mapper.hpp"

#include <algorithm>

namespace {

constexpr std::uint16_t vk_back = 0x08;
constexpr std::uint16_t vk_tab = 0x09;
constexpr std::uint16_t vk_clear = 0x0C;
constexpr std::uint16_t vk_return = 0x0D;
constexpr std::uint16_t vk_shift = 0x10;
constexpr std::uint16_t vk_control = 0x11;
constexpr std::uint16_t vk_alt = 0x12;
constexpr std::uint16_t vk_pause = 0x13;
constexpr std::uint16_t vk_caps_lock = 0x14;
constexpr std::uint16_t vk_escape = 0x1B;
constexpr std::uint16_t vk_space = 0x20;
constexpr std::uint16_t vk_page_up = 0x21;
constexpr std::uint16_t vk_page_down = 0x22;
constexpr std::uint16_t vk_end = 0x23;
constexpr std::uint16_t vk_home = 0x24;
constexpr std::uint16_t vk_left = 0x25;
constexpr std::uint16_t vk_up = 0x26;
constexpr std::uint16_t vk_right = 0x27;
constexpr std::uint16_t vk_down = 0x28;
constexpr std::uint16_t vk_print_screen = 0x2C;
constexpr std::uint16_t vk_insert = 0x2D;
constexpr std::uint16_t vk_delete = 0x2E;
constexpr std::uint16_t vk_0 = 0x30;
constexpr std::uint16_t vk_a = 0x41;
constexpr std::uint16_t vk_lwin = 0x5B;
constexpr std::uint16_t vk_rwin = 0x5C;
constexpr std::uint16_t vk_apps = 0x5D;
constexpr std::uint16_t vk_numpad_0 = 0x60;
constexpr std::uint16_t vk_multiply = 0x6A;
constexpr std::uint16_t vk_add = 0x6B;
constexpr std::uint16_t vk_separator = 0x6C;
constexpr std::uint16_t vk_subtract = 0x6D;
constexpr std::uint16_t vk_decimal = 0x6E;
constexpr std::uint16_t vk_divide = 0x6F;
constexpr std::uint16_t vk_f1 = 0x70;
constexpr std::uint16_t vk_num_lock = 0x90;
constexpr std::uint16_t vk_scroll_lock = 0x91;
constexpr std::uint16_t vk_lshift = 0xA0;
constexpr std::uint16_t vk_rshift = 0xA1;
constexpr std::uint16_t vk_lcontrol = 0xA2;
constexpr std::uint16_t vk_rcontrol = 0xA3;
constexpr std::uint16_t vk_lalt = 0xA4;
constexpr std::uint16_t vk_ralt = 0xA5;

#ifdef Q_OS_WIN
std::optional<std::uint16_t> windowsScanCodeToVirtualKey(const quint32 native_scan_code) noexcept {
  const auto extended = (native_scan_code & 0x100U) != 0 || (native_scan_code & 0xE000U) == 0xE000U;
  const auto scan_code = native_scan_code & 0xFFU;
  if (extended) {
    switch (scan_code) {
      case 0x1C:
        return vk_return;
      case 0x1D:
        return vk_rcontrol;
      case 0x35:
        return vk_divide;
      case 0x37:
        return vk_print_screen;
      case 0x38:
        return vk_ralt;
      case 0x47:
        return vk_home;
      case 0x48:
        return vk_up;
      case 0x49:
        return vk_page_up;
      case 0x4B:
        return vk_left;
      case 0x4D:
        return vk_right;
      case 0x4F:
        return vk_end;
      case 0x50:
        return vk_down;
      case 0x51:
        return vk_page_down;
      case 0x52:
        return vk_insert;
      case 0x53:
        return vk_delete;
      case 0x5B:
        return vk_lwin;
      case 0x5C:
        return vk_rwin;
      case 0x5D:
        return vk_apps;
      default:
        return std::nullopt;
    }
  }

  switch (scan_code) {
    case 0x01: return vk_escape;
    case 0x02: return '1';
    case 0x03: return '2';
    case 0x04: return '3';
    case 0x05: return '4';
    case 0x06: return '5';
    case 0x07: return '6';
    case 0x08: return '7';
    case 0x09: return '8';
    case 0x0A: return '9';
    case 0x0B: return '0';
    case 0x0C: return 0xBD;
    case 0x0D: return 0xBB;
    case 0x0E: return vk_back;
    case 0x0F: return vk_tab;
    case 0x10: return 'Q';
    case 0x11: return 'W';
    case 0x12: return 'E';
    case 0x13: return 'R';
    case 0x14: return 'T';
    case 0x15: return 'Y';
    case 0x16: return 'U';
    case 0x17: return 'I';
    case 0x18: return 'O';
    case 0x19: return 'P';
    case 0x1A: return 0xDB;
    case 0x1B: return 0xDD;
    case 0x1C: return vk_return;
    case 0x1D: return vk_lcontrol;
    case 0x1E: return 'A';
    case 0x1F: return 'S';
    case 0x20: return 'D';
    case 0x21: return 'F';
    case 0x22: return 'G';
    case 0x23: return 'H';
    case 0x24: return 'J';
    case 0x25: return 'K';
    case 0x26: return 'L';
    case 0x27: return 0xBA;
    case 0x28: return 0xDE;
    case 0x29: return 0xC0;
    case 0x2A: return vk_lshift;
    case 0x2B: return 0xDC;
    case 0x2C: return 'Z';
    case 0x2D: return 'X';
    case 0x2E: return 'C';
    case 0x2F: return 'V';
    case 0x30: return 'B';
    case 0x31: return 'N';
    case 0x32: return 'M';
    case 0x33: return 0xBC;
    case 0x34: return 0xBE;
    case 0x35: return 0xBF;
    case 0x36: return vk_rshift;
    case 0x37: return vk_multiply;
    case 0x38: return vk_lalt;
    case 0x39: return vk_space;
    case 0x3A: return vk_caps_lock;
    case 0x3B: return vk_f1;
    case 0x3C: return vk_f1 + 1;
    case 0x3D: return vk_f1 + 2;
    case 0x3E: return vk_f1 + 3;
    case 0x3F: return vk_f1 + 4;
    case 0x40: return vk_f1 + 5;
    case 0x41: return vk_f1 + 6;
    case 0x42: return vk_f1 + 7;
    case 0x43: return vk_f1 + 8;
    case 0x44: return vk_f1 + 9;
    case 0x45: return vk_num_lock;
    case 0x46: return vk_scroll_lock;
    case 0x47: return vk_numpad_0 + 7;
    case 0x48: return vk_numpad_0 + 8;
    case 0x49: return vk_numpad_0 + 9;
    case 0x4A: return vk_subtract;
    case 0x4B: return vk_numpad_0 + 4;
    case 0x4C: return vk_numpad_0 + 5;
    case 0x4D: return vk_numpad_0 + 6;
    case 0x4E: return vk_add;
    case 0x4F: return vk_numpad_0 + 1;
    case 0x50: return vk_numpad_0 + 2;
    case 0x51: return vk_numpad_0 + 3;
    case 0x52: return vk_numpad_0;
    case 0x53: return vk_decimal;
    case 0x57: return vk_f1 + 10;
    case 0x58: return vk_f1 + 11;
    default: return std::nullopt;
  }
}
#endif

std::optional<std::uint16_t> qtKeyToVirtualKey(
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept {
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    return vk_a + static_cast<std::uint16_t>(key - Qt::Key_A);
  }
  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    return (modifiers & Qt::KeypadModifier) != 0
               ? vk_numpad_0 + static_cast<std::uint16_t>(key - Qt::Key_0)
               : vk_0 + static_cast<std::uint16_t>(key - Qt::Key_0);
  }
  if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
    return vk_f1 + static_cast<std::uint16_t>(key - Qt::Key_F1);
  }

  const auto keypad = (modifiers & Qt::KeypadModifier) != 0;
  switch (key) {
    case Qt::Key_Backspace: return vk_back;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: return vk_tab;
    case Qt::Key_Clear: return vk_clear;
    case Qt::Key_Return:
    case Qt::Key_Enter: return vk_return;
    case Qt::Key_Shift: return vk_shift;
    case Qt::Key_Control: return vk_control;
    case Qt::Key_Alt: return vk_alt;
    case Qt::Key_AltGr: return vk_ralt;
    case Qt::Key_Meta: return vk_lwin;
    case Qt::Key_Menu: return vk_apps;
    case Qt::Key_Pause: return vk_pause;
    case Qt::Key_CapsLock: return vk_caps_lock;
    case Qt::Key_Escape: return vk_escape;
    case Qt::Key_Space: return vk_space;
    case Qt::Key_PageUp: return vk_page_up;
    case Qt::Key_PageDown: return vk_page_down;
    case Qt::Key_End: return vk_end;
    case Qt::Key_Home: return vk_home;
    case Qt::Key_Left: return vk_left;
    case Qt::Key_Up: return vk_up;
    case Qt::Key_Right: return vk_right;
    case Qt::Key_Down: return vk_down;
    case Qt::Key_Print: return vk_print_screen;
    case Qt::Key_Insert: return vk_insert;
    case Qt::Key_Delete: return vk_delete;
    case Qt::Key_Asterisk: return keypad ? vk_multiply : std::optional<std::uint16_t> {};
    case Qt::Key_Plus: return keypad ? vk_add : 0xBB;
    case Qt::Key_Comma: return keypad ? vk_separator : 0xBC;
    case Qt::Key_Minus: return keypad ? vk_subtract : 0xBD;
    case Qt::Key_Period: return keypad ? vk_decimal : 0xBE;
    case Qt::Key_Slash: return keypad ? vk_divide : 0xBF;
    case Qt::Key_NumLock: return vk_num_lock;
    case Qt::Key_ScrollLock: return vk_scroll_lock;
    case Qt::Key_Semicolon: return 0xBA;
    case Qt::Key_Equal: return 0xBB;
    case Qt::Key_QuoteLeft: return 0xC0;
    case Qt::Key_BracketLeft: return 0xDB;
    case Qt::Key_Backslash: return 0xDC;
    case Qt::Key_BracketRight: return 0xDD;
    case Qt::Key_Apostrophe: return 0xDE;
    default: return std::nullopt;
  }
}

bool contains(const std::span<const std::uint16_t> keys, const std::uint16_t key) noexcept {
  return std::ranges::find(keys, key) != keys.end();
}

}  // namespace

std::optional<QtKeyMapping> mapQtKey(
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const quint32 native_virtual_key,
    const quint32 native_scan_code) noexcept {
#ifdef Q_OS_WIN
  if (const auto mapped = windowsScanCodeToVirtualKey(native_scan_code); mapped.has_value()) {
    return QtKeyMapping {.virtual_key = *mapped, .normalized = true};
  }
#endif
  if (const auto mapped = qtKeyToVirtualKey(key, modifiers); mapped.has_value()) {
    return QtKeyMapping {.virtual_key = *mapped, .normalized = true};
  }
  if (native_virtual_key > 0 && native_virtual_key <= 0xFFU) {
    return QtKeyMapping {.virtual_key = static_cast<std::uint16_t>(native_virtual_key), .normalized = false};
  }
  return std::nullopt;
}

one::protocol::KeyModifiers keyModifiersForPressedKeys(
    const std::span<const std::uint16_t> pressed_keys) noexcept {
  return {
      .shift = contains(pressed_keys, vk_shift) || contains(pressed_keys, vk_lshift) ||
               contains(pressed_keys, vk_rshift),
      .control = contains(pressed_keys, vk_control) || contains(pressed_keys, vk_lcontrol) ||
                 contains(pressed_keys, vk_rcontrol),
      .alt = contains(pressed_keys, vk_alt) || contains(pressed_keys, vk_lalt) || contains(pressed_keys, vk_ralt),
      .meta = contains(pressed_keys, vk_lwin) || contains(pressed_keys, vk_rwin),
  };
}

bool isReleaseCaptureShortcut(
    const std::uint16_t virtual_key,
    const one::protocol::KeyModifiers modifiers) noexcept {
  return virtual_key == 'Z' && modifiers.shift && modifiers.control && modifiers.alt;
}
