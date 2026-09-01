#include <array>
#include <iostream>

#include "qt_input_mapper.hpp"

namespace {

int expect(const bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << message << '\n';
  return 1;
}

}  // namespace

int main() {
  int failures = 0;

  const auto letter = mapQtKey(Qt::Key_A, Qt::NoModifier, 0, 0);
  failures += expect(letter.has_value() && letter->virtual_key == 0x41, "letters should map to Win32 VK values");

  const auto function = mapQtKey(Qt::Key_F24, Qt::NoModifier, 0, 0);
  failures += expect(function.has_value() && function->virtual_key == 0x87, "F1 through F24 should map contiguously");

  const auto numpad = mapQtKey(Qt::Key_7, Qt::KeypadModifier, 0, 0);
  failures += expect(numpad.has_value() && numpad->virtual_key == 0x67, "keypad digits should remain distinct");

#ifdef Q_OS_WIN
  const auto physical_a = mapQtKey(Qt::Key_Q, Qt::NoModifier, 'Q', 0x1E);
  failures += expect(
      physical_a.has_value() && physical_a->virtual_key == 0x41 && physical_a->normalized,
      "Windows scan codes should provide QWERTY physical-key normalization");

  const auto right_alt = mapQtKey(Qt::Key_Alt, Qt::AltModifier, 0x12, 0x138);
  failures += expect(right_alt.has_value() && right_alt->virtual_key == 0xA5, "right Alt should retain its side");
#endif

  std::array<std::uint16_t, 2> both_alt {0xA4, 0xA5};
  failures += expect(keyModifiersForPressedKeys(both_alt).alt, "either Alt key should set the aggregate modifier");
  const std::array<std::uint16_t, 1> left_alt {0xA4};
  failures += expect(
      keyModifiersForPressedKeys(left_alt).alt,
      "releasing right Alt must not clear a still-pressed left Alt");

  const std::array<std::uint16_t, 3> release_keys {0xA0, 0xA2, 0xA4};
  failures += expect(
      isReleaseCaptureShortcut('Z', keyModifiersForPressedKeys(release_keys)),
      "Ctrl+Alt+Shift+Z should release input capture");

  return failures == 0 ? 0 : 1;
}
