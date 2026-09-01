#include "host_list_model.hpp"

#include <limits>
#include <utility>

#include <QString>

HostListModel::HostListModel(QObject* parent) : QAbstractListModel(parent) {}

int HostListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(catalog_.hosts().size());
}

QVariant HostListModel::data(const QModelIndex& index, const int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }

  const auto& host = catalog_.hosts()[static_cast<std::size_t>(index.row())];
  switch (role) {
    case DisplayNameRole:
      return QString::fromStdString(host.display_name);
    case AddressRole:
      return QString::fromStdString(host.endpoint.address);
    case PortRole:
      return host.endpoint.port;
    case OnlineRole:
      return host.reachability == one::HostReachability::online;
    case PairedRole:
      return host.pairing == one::HostPairingState::paired;
    case ManuallyAddedRole:
      return host.manually_added;
    default:
      return {};
  }
}

QHash<int, QByteArray> HostListModel::roleNames() const {
  return {
      {DisplayNameRole, "displayName"},
      {AddressRole, "address"},
      {PortRole, "port"},
      {OnlineRole, "online"},
      {PairedRole, "paired"},
      {ManuallyAddedRole, "manuallyAdded"},
  };
}

bool HostListModel::addManualHost(const QString& address, const int port) {
  const auto normalized_address = address.trimmed();
  if (normalized_address.isEmpty() || port <= 0 || port > std::numeric_limits<std::uint16_t>::max()) {
    return false;
  }

  one::HostEndpoint endpoint {
      .address = normalized_address.toStdString(),
      .port = static_cast<std::uint16_t>(port),
  };
  const auto new_host = catalog_.find_by_endpoint(endpoint) == nullptr;
  const auto next_row = rowCount();

  if (new_host) {
    beginInsertRows({}, next_row, next_row);
  }

  const auto index = catalog_.merge({
      .display_name = endpoint.address,
      .endpoint = std::move(endpoint),
      .origin = one::HostOrigin::manual,
      .reachability = one::HostReachability::unknown,
  });

  if (new_host) {
    endInsertRows();
    emit countChanged();
  } else {
    const auto model_index = createIndex(static_cast<int>(index), 0);
    emit dataChanged(model_index, model_index);
  }
  return true;
}
