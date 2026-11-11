#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <QObject>

#include "one/protocol/streaming_session.hpp"

class DesktopGamepadManager final : public QObject {
  Q_OBJECT

 public:
  explicit DesktopGamepadManager(QObject* parent = nullptr);
  ~DesktopGamepadManager() override;

  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] int controllerCount() const noexcept;
  [[nodiscard]] std::uint16_t activeMask() const noexcept;
  void attachSession(one::protocol::StreamingSession* session) noexcept;
  void rumble(
      std::uint16_t controllerNumber,
      std::uint16_t lowFrequencyMotor,
      std::uint16_t highFrequencyMotor) noexcept;
  void rumbleTriggers(
      std::uint16_t controllerNumber,
      std::uint16_t leftTriggerMotor,
      std::uint16_t rightTriggerMotor) noexcept;
  void setMotionReporting(
      std::uint16_t controllerNumber,
      one::protocol::ControllerMotionType type,
      std::uint16_t reportRateHz) noexcept;
  void setLed(
      std::uint16_t controllerNumber,
      std::uint8_t red,
      std::uint8_t green,
      std::uint8_t blue) noexcept;
  void setAdaptiveTriggers(
      std::uint16_t controllerNumber,
      const one::protocol::ControllerAdaptiveTriggerEffect& effect) noexcept;

 signals:
  void controllerCountChanged();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] one::protocol::ControllerButtonMask gamepadButtonForSdlButton(int button) noexcept;
[[nodiscard]] std::int16_t invertGamepadYAxis(std::int16_t value) noexcept;
[[nodiscard]] std::uint8_t gamepadTriggerValue(std::int16_t value) noexcept;

inline constexpr std::size_t dualsense_effect_packet_size = 47;
[[nodiscard]] std::array<std::uint8_t, dualsense_effect_packet_size> dualSenseEffectPacket(
    const one::protocol::ControllerAdaptiveTriggerEffect& effect) noexcept;
