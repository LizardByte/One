#pragma once

#include <QByteArray>
#include <QSslConfiguration>
#include <QString>

class ClientIdentity final {
 public:
  ClientIdentity();

  [[nodiscard]] const QString& uniqueId() const noexcept;
  [[nodiscard]] const QByteArray& certificatePem() const noexcept;
  [[nodiscard]] const QByteArray& privateKeyPem() const noexcept;
  [[nodiscard]] QSslConfiguration sslConfiguration() const;

 private:
  void createCredentials();
  void loadOrCreate();

  QString unique_id_;
  QByteArray certificate_pem_;
  QByteArray private_key_pem_;
};
