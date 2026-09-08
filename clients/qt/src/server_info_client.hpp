#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>

#include "server_info.hpp"

class ClientIdentity;

class ServerInfoClient final : public QObject {
  Q_OBJECT

 public:
  explicit ServerInfoClient(const ClientIdentity& identity, QObject* parent = nullptr);

  void query(const QString& address, quint16 port);
  void cancel(const QString& address, quint16 port);

 signals:
  void succeeded(const QString& address, quint16 port, const ServerInfo& info);
  void failed(const QString& address, quint16 port, const QString& error);
  // Emitted only after checking the pinned peer, never from plaintext PairStatus.
  void pairingStatus(const QString& stableId, bool paired);

 private:
  [[nodiscard]] static QString requestKey(const QString& address, quint16 port);
  void verifyPairing(const QString& address, quint16 port, const ServerInfo& info);

  const ClientIdentity& identity_;
  QNetworkAccessManager network_;
  QHash<QString, QNetworkReply*> pending_;
};

Q_DECLARE_METATYPE(ServerInfo)
