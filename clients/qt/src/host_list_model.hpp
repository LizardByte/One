#pragma once

#include <QAbstractListModel>

#include "one/host.hpp"

class HostListModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

 public:
  enum Role {
    DisplayNameRole = Qt::UserRole + 1,
    AddressRole,
    PortRole,
    OnlineRole,
    PairedRole,
    ManuallyAddedRole,
  };
  Q_ENUM(Role)

  explicit HostListModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE bool addManualHost(const QString& address, int port = 47989);

 signals:
  void countChanged();

 private:
  one::HostCatalog catalog_;
};
