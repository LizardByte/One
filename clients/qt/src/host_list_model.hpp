#pragma once

#include <memory>

#include <QAbstractListModel>
#include <QHash>

#include "one/host.hpp"
#include "server_info.hpp"

class HostListModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

 public:
  enum Role {
    DisplayNameRole = Qt::UserRole + 1,
    StableIdRole,
    AddressRole,
    PortRole,
    OnlineRole,
    PairedRole,
    PairingRole,
    ManuallyAddedRole,
    StatusRole,
  };
  Q_ENUM(Role)

  explicit HostListModel(QObject* parent = nullptr, bool enableNetworkServices = true);
  ~HostListModel() override;

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE bool addManualHost(const QString& address, int port = 47989);
  Q_INVOKABLE QString pairHost(int row);

 signals:
  void countChanged();
  void pairingCompleted(const QString& stableId, bool success, const QString& error);

 private:
  void mergeHost(one::HostUpdate update, bool queryServerInfo);
  void handleServerInfo(const QString& address, quint16 port, const struct ServerInfo& info);
  void handleServerInfoFailure(const QString& address, quint16 port);
  void handlePairingCompleted(const QString& stableId, const struct PairingResult& result);
  void emitHostChanged(const QString& stableId);

  one::HostCatalog catalog_;
  std::unique_ptr<class ClientIdentity> identity_;
  std::unique_ptr<class HostDiscoveryService> discovery_;
  std::unique_ptr<class PairingClient> pairing_;
  std::unique_ptr<class ServerInfoClient> server_info_;
  QHash<QString, ServerInfo> server_info_by_id_;
};
