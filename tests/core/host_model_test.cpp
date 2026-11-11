#include <iostream>
#include <stdexcept>

#include "one/host.hpp"

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
  one::HostCatalog catalog;

  const auto discovered_index = catalog.merge({
      .display_name = "Living Room",
      .endpoint = {"192.0.2.10", 47989},
      .origin = one::HostOrigin::discovered,
      .reachability = one::HostReachability::online,
  });
  failures += expect(discovered_index == 0 && catalog.hosts().size() == 1, "discovery should add a host");

  const auto identified_index = catalog.merge({
      .stable_id = "sunshine-uuid",
      .display_name = "Living Room PC",
      .endpoint = {"192.0.2.10", 47989},
      .origin = one::HostOrigin::discovered,
      .reachability = one::HostReachability::online,
  });
  failures += expect(identified_index == discovered_index, "server information should identify the discovered host");
  failures += expect(catalog.hosts().size() == 1, "identity promotion must not duplicate a host");
  failures += expect(catalog.find_by_id("sunshine-uuid") != nullptr, "promoted host should be found by stable ID");

  failures += expect(
      catalog.set_pairing_state("sunshine-uuid", one::HostPairingState::paired),
      "known host pairing state should update");

  const auto moved_index = catalog.merge({
      .stable_id = "sunshine-uuid",
      .display_name = "Living Room PC",
      .endpoint = {"192.0.2.25", 47989},
      .origin = one::HostOrigin::manual,
      .reachability = one::HostReachability::online,
  });
  const auto* moved = catalog.find_by_id("sunshine-uuid");
  failures += expect(moved_index == identified_index, "stable ID should survive an address change");
  failures += expect(moved != nullptr && moved->endpoint.address == "192.0.2.25", "host address should refresh");
  failures += expect(moved != nullptr && moved->pairing == one::HostPairingState::paired, "rediscovery must preserve pairing");
  failures += expect(moved != nullptr && moved->manually_added, "manual origin should be retained");

  try {
    static_cast<void>(catalog.merge({.endpoint = {"", 47989}}));
    failures += expect(false, "empty host address should be rejected");
  } catch (const std::invalid_argument&) {
  }

  return failures == 0 ? 0 : 1;
}
