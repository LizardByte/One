#include "one/pairing_session.hpp"

#include <stdexcept>
#include <utility>

namespace one {

PairingSession::PairingSession(std::string host_id) : host_id_(std::move(host_id)) {
  if (host_id_.empty()) {
    throw std::invalid_argument("pairing requires a stable host ID");
  }
}

bool PairingSession::begin(std::string pin) {
  if (!is_valid_pin(pin) || state_ == PairingSessionState::waiting_for_host ||
      state_ == PairingSessionState::verifying || state_ == PairingSessionState::paired) {
    return false;
  }

  pin_ = std::move(pin);
  error_.clear();
  state_ = PairingSessionState::waiting_for_host;
  return true;
}

bool PairingSession::mark_verifying() noexcept {
  if (state_ != PairingSessionState::waiting_for_host) {
    return false;
  }
  state_ = PairingSessionState::verifying;
  return true;
}

bool PairingSession::succeed() noexcept {
  if (state_ != PairingSessionState::waiting_for_host && state_ != PairingSessionState::verifying) {
    return false;
  }
  pin_.clear();
  error_.clear();
  state_ = PairingSessionState::paired;
  return true;
}

bool PairingSession::fail(std::string error) {
  if (state_ != PairingSessionState::waiting_for_host && state_ != PairingSessionState::verifying) {
    return false;
  }
  pin_.clear();
  error_ = std::move(error);
  state_ = PairingSessionState::failed;
  return true;
}

bool PairingSession::cancel() noexcept {
  if (state_ != PairingSessionState::waiting_for_host && state_ != PairingSessionState::verifying) {
    return false;
  }
  pin_.clear();
  error_.clear();
  state_ = PairingSessionState::cancelled;
  return true;
}

void PairingSession::reset() noexcept {
  pin_.clear();
  error_.clear();
  state_ = PairingSessionState::idle;
}

const std::string& PairingSession::host_id() const noexcept {
  return host_id_;
}

const std::string& PairingSession::pin() const noexcept {
  return pin_;
}

const std::string& PairingSession::error() const noexcept {
  return error_;
}

PairingSessionState PairingSession::state() const noexcept {
  return state_;
}

bool PairingSession::is_valid_pin(const std::string_view pin) noexcept {
  if (pin.size() != 4) {
    return false;
  }
  for (const char character : pin) {
    if (character < '0' || character > '9') {
      return false;
    }
  }
  return true;
}

}  // namespace one
