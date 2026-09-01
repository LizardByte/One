#include "one/host.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace one {
namespace {

constexpr auto missing_index = std::numeric_limits<std::size_t>::max();

void validate(const HostUpdate& update) {
  if (update.endpoint.address.empty()) {
    throw std::invalid_argument("host address must not be empty");
  }
  if (update.endpoint.port == 0) {
    throw std::invalid_argument("host port must not be zero");
  }
}

}  // namespace

std::size_t HostCatalog::merge(HostUpdate update) {
  validate(update);

  const auto index = find_merge_index(update);
  if (index == missing_index) {
    hosts_.push_back({
        .stable_id = std::move(update.stable_id),
        .display_name = update.display_name.empty() ? update.endpoint.address : std::move(update.display_name),
        .endpoint = std::move(update.endpoint),
        .reachability = update.reachability,
        .pairing = HostPairingState::unpaired,
        .manually_added = update.origin == HostOrigin::manual,
    });
    return hosts_.size() - 1;
  }

  auto& host = hosts_[index];
  if (!update.stable_id.empty()) {
    host.stable_id = std::move(update.stable_id);
  }
  if (!update.display_name.empty()) {
    host.display_name = std::move(update.display_name);
  }
  host.endpoint = std::move(update.endpoint);
  host.reachability = update.reachability;
  host.manually_added = host.manually_added || update.origin == HostOrigin::manual;
  return index;
}

const Host* HostCatalog::find_by_id(const std::string_view stable_id) const noexcept {
  if (stable_id.empty()) {
    return nullptr;
  }

  for (const auto& host : hosts_) {
    if (host.stable_id == stable_id) {
      return &host;
    }
  }
  return nullptr;
}

const Host* HostCatalog::find_by_endpoint(const HostEndpoint& endpoint) const noexcept {
  for (const auto& host : hosts_) {
    if (host.endpoint == endpoint) {
      return &host;
    }
  }
  return nullptr;
}

bool HostCatalog::set_pairing_state(const std::string_view stable_id, const HostPairingState state) noexcept {
  for (auto& host : hosts_) {
    if (!stable_id.empty() && host.stable_id == stable_id) {
      host.pairing = state;
      return true;
    }
  }
  return false;
}

const std::vector<Host>& HostCatalog::hosts() const noexcept {
  return hosts_;
}

std::size_t HostCatalog::find_merge_index(const HostUpdate& update) const noexcept {
  if (!update.stable_id.empty()) {
    for (std::size_t index = 0; index < hosts_.size(); ++index) {
      if (hosts_[index].stable_id == update.stable_id) {
        return index;
      }
    }
  }

  for (std::size_t index = 0; index < hosts_.size(); ++index) {
    if (hosts_[index].endpoint == update.endpoint) {
      return index;
    }
  }

  return missing_index;
}

}  // namespace one
