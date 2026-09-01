#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>

#include "server_info.hpp"

class ClientIdentity;

class ServerInfoClient final : public QObject {
  Q_OBJECT

 public:
  explicit ServerInfoClient(const ClientIdentity& identity, QObject* parent = nullptr);

  void query(const QString& address, quint16 port);

 signals:
  void succeeded(const QString& address, quint16 port, const ServerInfo& info);
  void failed(const QString& address, quint16 port, const QString& error);

 private:
  [[nodiscard]] static QString requestKey(const QString& address, quint16 port);

  const ClientIdentity& identity_;
  QNetworkAccessManager network_;
  QSet<QString> pending_;
};

Q_DECLARE_METATYPE(ServerInfo)
