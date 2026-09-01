#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace one {

enum class HostReachability {
  unknown,
  offline,
  online,
};

enum class HostPairingState {
  unpaired,
  pairing,
  paired,
};

enum class HostOrigin {
  discovered,
  manual,
};

struct HostEndpoint {
  std::string address;
  std::uint16_t port {};

  bool operator==(const HostEndpoint&) const = default;
};

struct HostUpdate {
  std::string stable_id;
  std::string display_name;
  HostEndpoint endpoint;
  HostOrigin origin {HostOrigin::discovered};
  HostReachability reachability {HostReachability::unknown};
  std::optional<HostPairingState> pairing;
};

struct Host {
  std::string stable_id;
  std::string display_name;
  HostEndpoint endpoint;
  HostReachability reachability {HostReachability::unknown};
  HostPairingState pairing {HostPairingState::unpaired};
  bool manually_added {false};
};

class HostCatalog {
 public:
  [[nodiscard]] std::size_t merge(HostUpdate update);
  [[nodiscard]] const Host* find_by_id(std::string_view stable_id) const noexcept;
  [[nodiscard]] const Host* find_by_endpoint(const HostEndpoint& endpoint) const noexcept;
  [[nodiscard]] bool set_pairing_state(std::string_view stable_id, HostPairingState state) noexcept;
  [[nodiscard]] const std::vector<Host>& hosts() const noexcept;

 private:
  [[nodiscard]] std::size_t find_merge_index(const HostUpdate& update) const noexcept;

  std::vector<Host> hosts_;
};

}  // namespace one
