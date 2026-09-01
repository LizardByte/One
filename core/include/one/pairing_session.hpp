#pragma once

#include <string>
#include <string_view>

namespace one {

enum class PairingSessionState {
  idle,
  waiting_for_host,
  verifying,
  paired,
  failed,
  cancelled,
};

class PairingSession {
 public:
  explicit PairingSession(std::string host_id);

  [[nodiscard]] bool begin(std::string pin);
  [[nodiscard]] bool mark_verifying() noexcept;
  [[nodiscard]] bool succeed() noexcept;
  [[nodiscard]] bool fail(std::string error);
  [[nodiscard]] bool cancel() noexcept;
  void reset() noexcept;

  [[nodiscard]] const std::string& host_id() const noexcept;
  [[nodiscard]] const std::string& pin() const noexcept;
  [[nodiscard]] const std::string& error() const noexcept;
  [[nodiscard]] PairingSessionState state() const noexcept;
  [[nodiscard]] static bool is_valid_pin(std::string_view pin) noexcept;

 private:
  std::string host_id_;
  std::string pin_;
  std::string error_;
  PairingSessionState state_ {PairingSessionState::idle};
};

}  // namespace one
