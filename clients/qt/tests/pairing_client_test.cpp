#include "pairing_client.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QSettings>
#include <QSslKey>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

#include "application_list_model.hpp"
#include "client_identity.hpp"
#include "host_list_model.hpp"

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("LizardByteTest"));
  QCoreApplication::setApplicationName(QStringLiteral("OnePairingTest"));

  QTemporaryDir settings_directory;
  if (!settings_directory.isValid()) {
    std::cerr << "unable to create temporary identity settings\n";
    return 1;
  }
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_directory.path());

  const ClientIdentity first_identity;
  const ClientIdentity second_identity;
  // Link the same shared client models as the application. This also catches
  // inconsistent Qt inline-container implementations across translation units.
  HostListModel hosts(nullptr, false);
  ApplicationListModel applications;
  if (first_identity.uniqueId().size() != 16 || first_identity.uniqueId() != second_identity.uniqueId() ||
      first_identity.certificatePem() != second_identity.certificatePem() ||
      first_identity.privateKeyPem() != second_identity.privateKeyPem()) {
    std::cerr << "client identity must be complete and persistent\n";
    return 1;
  }
  const auto ssl_configuration = first_identity.sslConfiguration();
  if (ssl_configuration.localCertificate().isNull() || ssl_configuration.privateKey().isNull()) {
    std::cerr << "client identity must produce a usable TLS configuration\n";
    return 1;
  }

  for (int iteration = 0; iteration < 100; ++iteration) {
    const auto pin = PairingClient::generatePin();
    if (pin.size() != 4) {
      std::cerr << "pairing PIN must contain exactly four digits\n";
      return 1;
    }
    for (const auto character : pin) {
      if (!character.isDigit()) {
        std::cerr << "pairing PIN must contain only digits\n";
        return 1;
      }
    }
  }

  // Exercise the actual asynchronous pairing worker, not just PIN generation.
  // A rejected exchange must report a normal failure, never crash constructing
  // its query parameters. No real host or saved pairing is touched.
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost)) return 1;
  int requests = 0;
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
    while (server.hasPendingConnections()) {
      auto* socket = server.nextPendingConnection();
      QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, data = QByteArray()]() mutable {
        data += socket->readAll();
        if (!data.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
        socket->setProperty("answered", true);
        ++requests;
        const QByteArray body = "<root status_code=\"200\"><paired>0</paired></root>";
        socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) +
                      "\r\n\r\n" + body);
        socket->disconnectFromHost();
      });
    }
  });
  PairingClient pairing(first_identity);
  QEventLoop loop;
  bool completed = false;
  QObject::connect(&pairing, &PairingClient::completed, &loop, [&](const QString& id, const PairingResult& result) {
    completed = id == "test-instance" && !result.success && !result.error.isEmpty();
    loop.quit();
  });
  QTimer::singleShot(10000, &loop, &QEventLoop::quit);
  if (pairing
          .start({.stable_id = "test-instance",
                  .address = "127.0.0.1",
                  .app_version = "7.1.0",
                  .http_port = server.serverPort()})
          .isEmpty())
    return 1;
  loop.exec();
  if (!completed || requests != 2) {
    std::cerr << "pairing worker must report rejection and clean up its test exchange\n";
    return 1;
  }
  return 0;
}
