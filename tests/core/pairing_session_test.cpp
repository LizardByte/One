#include <iostream>

#include "one/pairing_session.hpp"

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
  one::PairingSession pairing("sunshine-uuid");

  failures += expect(!pairing.begin("12ab"), "pairing PIN must contain four digits");
  failures += expect(pairing.begin("0042"), "valid pairing PIN should begin the session");
  failures += expect(pairing.state() == one::PairingSessionState::waiting_for_host, "session should wait for host entry");
  failures += expect(!pairing.begin("1234"), "a running pairing attempt should not be replaced");
  failures += expect(pairing.mark_verifying(), "waiting session should enter verification");
  failures += expect(pairing.fail("certificate mismatch"), "active pairing should accept a failure");
  failures += expect(pairing.pin().empty(), "failed pairing should discard its PIN");
  failures += expect(pairing.error() == "certificate mismatch", "pairing failure should retain its diagnostic");

  failures += expect(pairing.begin("9876"), "failed session should be retryable");
  failures += expect(pairing.succeed(), "active pairing should complete");
  failures += expect(pairing.state() == one::PairingSessionState::paired, "successful pairing should become paired");
  failures += expect(!pairing.cancel(), "completed pairing should not be cancellable");

  pairing.reset();
  failures += expect(pairing.state() == one::PairingSessionState::idle, "reset should return to idle");
  failures += expect(pairing.begin("1234") && pairing.cancel(), "active pairing should be cancellable");

  return failures == 0 ? 0 : 1;
}
