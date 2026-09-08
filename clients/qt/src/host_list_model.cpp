#include "host_list_model.hpp"

#include <QString>
#include <QTimer>
#include <QUrl>
#include <limits>
#include <utility>

#include "client_identity.hpp"
#include "host_discovery_service.hpp"
#include "host_settings.hpp"
#include "pairing_client.hpp"
#include "server_info_client.hpp"

HostListModel::HostListModel(QObject* parent, const bool enableNetworkServices) : QAbstractListModel(parent) {
  if (!enableNetworkServices) {
    return;
  }

  identity_ = std::make_unique<ClientIdentity>();
  pairing_ = std::make_unique<PairingClient>(*identity_);
  server_info_ = std::make_unique<ServerInfoClient>(*identity_);
  discovery_ = std::make_unique<HostDiscoveryService>();

  connect(discovery_.get(), &HostDiscoveryService::hostDiscovered, this,
          [this](const QString& name, const QString& address, const quint16 port) {
            mergeHost(
                {
                    .display_name = name.toStdString(),
                    .endpoint = {address.toStdString(), port},
                    .origin = one::HostOrigin::discovered,
                    .reachability = one::HostReachability::unknown,
                },
                true);
          });
  connect(server_info_.get(), &ServerInfoClient::succeeded, this, &HostListModel::handleServerInfo);
  connect(
      server_info_.get(), &ServerInfoClient::failed, this,
      [this](const QString& address, const quint16 port, const QString&) { handleServerInfoFailure(address, port); });
  connect(pairing_.get(), &PairingClient::completed, this, &HostListModel::handlePairingCompleted);
  restoreHosts();
  refresh();
  auto* refresh_timer = new QTimer(this);
  connect(refresh_timer, &QTimer::timeout, this, &HostListModel::refresh);
  refresh_timer->start(15000);
  discovery_->start();
}

HostListModel::~HostListModel() = default;

void HostListModel::restoreHosts() {
  const auto saved = loadKnownHosts();
  beginResetModel();
  for (const auto& host : saved) {
    static_cast<void>(
        catalog_.merge({.stable_id = host.stable_id,
                        .display_name = host.display_name,
                        .endpoint = host.endpoint,
                        .origin = host.manually_added ? one::HostOrigin::manual : one::HostOrigin::discovered,
                        .reachability = one::HostReachability::unknown,
                        .pairing = host.pairing}));
  }
  endResetModel();
  emit countChanged();
}

void HostListModel::refresh() {
  if (!server_info_) return;
  for (const auto& host : catalog_.hosts()) {
    // Pairing owns the handshake until its worker finishes.
    if (host.pairing != one::HostPairingState::pairing)
      server_info_->query(QString::fromStdString(host.endpoint.address), host.endpoint.port);
  }
}

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
    case StableIdRole:
      return QString::fromStdString(host.stable_id);
    case AddressRole:
      return QString::fromStdString(host.endpoint.address);
    case PortRole:
      return host.endpoint.port;
    case OnlineRole:
      return host.reachability == one::HostReachability::online;
    case PairedRole:
      return host.pairing == one::HostPairingState::paired;
    case PairingRole:
      return host.pairing == one::HostPairingState::pairing;
    case ManuallyAddedRole:
      return host.manually_added;
    case StatusRole:
      switch (host.reachability) {
        case one::HostReachability::online:
          return tr("Online");
        case one::HostReachability::offline:
          return tr("Offline");
        case one::HostReachability::unknown:
          return tr("Checking…");
      }
      return {};
    default:
      return {};
  }
}

QHash<int, QByteArray> HostListModel::roleNames() const {
  return {
      {DisplayNameRole, "displayName"}, {StableIdRole, "stableId"},
      {AddressRole, "address"},         {PortRole, "port"},
      {OnlineRole, "online"},           {PairedRole, "paired"},
      {PairingRole, "pairing"},         {ManuallyAddedRole, "manuallyAdded"},
      {StatusRole, "status"},
  };
}

bool HostListModel::addManualHost(const QString& address, const int port) {
  auto normalized_address = address.trimmed();
  int instance_port = port;
  if (normalized_address.startsWith(u'[') || normalized_address.count(u':') == 1) {
    const QUrl endpoint_url(QStringLiteral("http://") + normalized_address);
    if (!endpoint_url.isValid() || endpoint_url.host().isEmpty() || !endpoint_url.userInfo().isEmpty() ||
        !endpoint_url.path().isEmpty() || endpoint_url.hasQuery() || endpoint_url.hasFragment())
      return false;
    normalized_address = endpoint_url.host();
    instance_port = endpoint_url.port(port);
  }
  if (normalized_address.isEmpty() || instance_port <= 0 || instance_port > std::numeric_limits<std::uint16_t>::max()) {
    return false;
  }

  one::HostEndpoint endpoint{
      .address = normalized_address.toStdString(),
      .port = static_cast<std::uint16_t>(instance_port),
  };
  mergeHost(
      {
          .display_name = endpoint.address,
          .endpoint = std::move(endpoint),
          .origin = one::HostOrigin::manual,
          .reachability = one::HostReachability::unknown,
      },
      true);
  return true;
}

QString HostListModel::pairHost(const int row) {
  if (pairing_ == nullptr || row < 0 || row >= rowCount()) {
    return {};
  }

  const auto& host = catalog_.hosts()[static_cast<std::size_t>(row)];
  if (host.reachability != one::HostReachability::online || host.stable_id.empty() ||
      host.pairing != one::HostPairingState::unpaired) {
    return {};
  }

  const auto stable_id = QString::fromStdString(host.stable_id);
  const auto server_info = server_info_by_id_.constFind(stable_id);
  if (server_info == server_info_by_id_.constEnd()) {
    return {};
  }

  const auto pin = pairing_->start({
      .stable_id = stable_id,
      .address = QString::fromStdString(host.endpoint.address),
      .app_version = server_info->app_version,
      .http_port = host.endpoint.port,
      .https_port = server_info->https_port,
  });
  if (pin.isEmpty()) {
    return {};
  }

  static_cast<void>(catalog_.set_pairing_state(host.stable_id, one::HostPairingState::pairing));
  const auto model_index = index(row);
  emit dataChanged(model_index, model_index, {PairedRole, PairingRole});
  return pin;
}

bool HostListModel::openHost(const int row) {
  if (row < 0 || row >= rowCount()) {
    return false;
  }

  const auto& host = catalog_.hosts()[static_cast<std::size_t>(row)];
  if (host.reachability != one::HostReachability::online || host.pairing != one::HostPairingState::paired ||
      host.stable_id.empty()) {
    return false;
  }

  const auto stable_id = QString::fromStdString(host.stable_id);
  const auto server_info = server_info_by_id_.constFind(stable_id);
  if (server_info == server_info_by_id_.constEnd()) {
    return false;
  }

  emit hostOpened(stable_id, QString::fromStdString(host.display_name), QString::fromStdString(host.endpoint.address),
                  server_info->https_port, server_info->app_version, server_info->gfe_version,
                  server_info->codec_mode_support, server_info->current_game_id);
  return true;
}

void HostListModel::mergeHost(one::HostUpdate update, const bool queryServerInfo) {
  const auto address = QString::fromStdString(update.endpoint.address);
  const auto port = update.endpoint.port;
  const auto new_host = catalog_.find_by_endpoint(update.endpoint) == nullptr &&
                        (update.stable_id.empty() || catalog_.find_by_id(update.stable_id) == nullptr);
  const auto next_row = rowCount();
  if (new_host) {
    beginInsertRows({}, next_row, next_row);
  }

  const auto index = catalog_.merge(std::move(update));
  saveKnownHosts(catalog_.hosts());
  if (new_host) {
    endInsertRows();
    emit countChanged();
  } else {
    const auto model_index = createIndex(static_cast<int>(index), 0);
    emit dataChanged(model_index, model_index);
  }

  if (queryServerInfo && server_info_) {
    server_info_->query(address, port);
  }
}

void HostListModel::handleServerInfo(const QString& address, const quint16 port, const ServerInfo& info) {
  const auto instance_id = hostInstanceId(info.stable_id, port);
  migrateLegacyHostCertificate(info.stable_id, port);
  server_info_by_id_.insert(instance_id, info);

  auto pairing_state = one::HostPairingState::unpaired;
  const auto stable_id = instance_id.toStdString();
  if (const auto* existing = catalog_.find_by_id(stable_id);
      existing != nullptr && existing->pairing == one::HostPairingState::pairing) {
    pairing_state = one::HostPairingState::pairing;
  } else if (!hostNeedsPairing(instance_id) && !loadServerCertificate(instance_id).isEmpty()) {
    pairing_state = one::HostPairingState::paired;
  }

  mergeHost(
      {
          .stable_id = stable_id,
          .display_name = info.display_name.toStdString(),
          .endpoint = {address.toStdString(), port},
          .origin = one::HostOrigin::discovered,
          .reachability = one::HostReachability::online,
          .pairing = pairing_state,
      },
      false);
}

void HostListModel::handlePairingCompleted(const QString& stable_id, const PairingResult& result) {
  if (result.success) {
    saveServerCertificate(stable_id, result.server_certificate);
  }
  static_cast<void>(catalog_.set_pairing_state(
      stable_id.toStdString(), result.success ? one::HostPairingState::paired : one::HostPairingState::unpaired));
  emitHostChanged(stable_id);
  emit pairingCompleted(stable_id, result.success, result.error);
}

void HostListModel::requirePairing(const QString& stable_id) {
  setHostNeedsPairing(stable_id, true);
  static_cast<void>(catalog_.set_pairing_state(stable_id.toStdString(), one::HostPairingState::unpaired));
  emitHostChanged(stable_id);
}

void HostListModel::emitHostChanged(const QString& stable_id) {
  const auto& hosts = catalog_.hosts();
  for (std::size_t row = 0; row < hosts.size(); ++row) {
    if (hosts[row].stable_id == stable_id.toStdString()) {
      const auto model_index = index(static_cast<int>(row));
      emit dataChanged(model_index, model_index, {PairedRole, PairingRole});
      return;
    }
  }
}

void HostListModel::handleServerInfoFailure(const QString& address, const quint16 port) {
  mergeHost(
      {
          .endpoint = {address.toStdString(), port},
          .origin = one::HostOrigin::discovered,
          .reachability = one::HostReachability::offline,
      },
      false);
}
