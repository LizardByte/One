#include <iostream>

#include <QCoreApplication>
#include <QSettings>
#include <QSslKey>
#include <QString>
#include <QTemporaryDir>

#include "client_identity.hpp"
#include "pairing_client.hpp"

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
  return 0;
}
