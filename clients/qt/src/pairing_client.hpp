#pragma once

#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QString>

struct PairingTarget {
  QString stable_id;
  QString address;
  QString app_version;
  quint16 http_port {47989};
  quint16 https_port {47984};
};

struct PairingResult {
  bool success {false};
  QString error;
  QByteArray server_certificate;
};

class ClientIdentity;

class PairingClient final : public QObject {
  Q_OBJECT

 public:
  explicit PairingClient(const ClientIdentity& identity, QObject* parent = nullptr);

  [[nodiscard]] QString start(const PairingTarget& target);
  [[nodiscard]] static QString generatePin();

 signals:
  void completed(const QString& stableId, const PairingResult& result);

 private:
  QString unique_id_;
  QByteArray certificate_pem_;
  QByteArray private_key_pem_;
  QSet<QString> active_hosts_;
};

Q_DECLARE_METATYPE(PairingResult)
