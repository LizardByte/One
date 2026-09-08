#include "host_list_model.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>

#include "host_settings.hpp"
#include "pairing_client.hpp"

namespace {

int expect(const bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << message << '\n';
  return 1;
}

}  // namespace

class HostListModelTest {
 public:
  static int persistence() {
    QSettings().clear();
    int failures = 0;
    const auto id = hostInstanceId("saved-uuid", 47989);
    saveServerCertificate(id, "saved certificate");
    const ServerInfo info{.stable_id = "saved-uuid", .display_name = "Gaming PC", .https_port = 47984};
    {
      HostListModel original(nullptr, false);
      original.handleServerInfo("192.0.2.10", 47989, info);
      original.addManualHost("192.0.2.10:48089");
      original.addManualHost("[2001:db8::1]:48189");
      original.handleServerInfoFailure("192.0.2.10", 47989);
      failures +=
          expect(original.rowCount() == 3 && original.data(original.index(0), HostListModel::StatusRole) == "Offline" &&
                     original.data(original.index(0), HostListModel::PairedRole).toBool(),
                 "offline hosts must remain listed without losing pairing");
    }
    HostListModel restarted(nullptr, false);
    restarted.restoreHosts();
    failures += expect(restarted.rowCount() == 3 &&
                           restarted.data(restarted.index(0), HostListModel::DisplayNameRole) == "Gaming PC" &&
                           restarted.data(restarted.index(0), HostListModel::PairedRole).toBool() &&
                           restarted.data(restarted.index(0), HostListModel::StatusRole) == "Checking…" &&
                           !restarted.openHost(0),
                       "restart must restore known hosts and trust, but never stale online state");
    failures += expect(restarted.data(restarted.index(1), HostListModel::PortRole).toInt() == 48089 &&
                           restarted.data(restarted.index(1), HostListModel::ManuallyAddedRole).toBool() &&
                           restarted.data(restarted.index(2), HostListModel::AddressRole) == "2001:db8::1",
                       "unpaired manual entries, IPv6 and separate instance ports must survive restart");
    restarted.handleServerInfoFailure("192.0.2.10", 47989);
    restarted.handleServerInfo("192.0.2.11", 47989, info);
    failures += expect(restarted.rowCount() == 3 && restarted.openHost(0) &&
                           restarted.data(restarted.index(0), HostListModel::AddressRole) == "192.0.2.11",
                       "rediscovery must bring an offline instance online without duplicating it");
    restarted.requirePairing(id);
    HostListModel rejected(nullptr, false);
    rejected.restoreHosts();
    failures += expect(!rejected.data(rejected.index(0), HostListModel::PairedRole).toBool(),
                       "restored hosts must respect persisted certificate rejection");
    const auto saved = loadKnownHosts();
    failures += expect(saved.size() == 3 && saved[0].endpoint.address == "192.0.2.11",
                       "updated endpoints must be saved for the next startup");
    auto entries = QSettings().value("knownHosts").toList();
    entries.append(QVariantMap{{"address", "bad.test"}, {"port", 65536}});
    entries.append(QVariantMap{{"address", ""}, {"port", 47989}});
    QSettings().setValue("knownHosts", entries);
    failures += expect(loadKnownHosts().size() == 3, "invalid saved endpoints must be skipped");
    return failures;
  }

  static int instances() {
    HostListModel model(nullptr, false);
    int failures = 0;
    const auto primary_id = hostInstanceId("shared-uuid", 47989);
    const auto secondary_id = hostInstanceId("shared-uuid", 48089);
    saveServerCertificate(primary_id, "primary certificate");
    saveServerCertificate(secondary_id, "secondary certificate");
    ServerInfo primary{.stable_id = "shared-uuid", .display_name = "Archer", .https_port = 47984};
    ServerInfo secondary = primary;
    secondary.https_port = 48084;
    model.handleServerInfo("192.0.2.1", 47989, primary);
    model.handleServerInfo("192.0.2.1", 48089, secondary);
    failures += expect(model.rowCount() == 2,
                       "same address and UUID with different ports must remain "
                       "separate instances");
    failures += expect(model.data(model.index(0), HostListModel::StableIdRole) == primary_id &&
                           model.data(model.index(1), HostListModel::StableIdRole) == secondary_id,
                       "instance IDs must include each HTTP port");
    QString opened_id;
    quint16 opened_port = 0;
    QObject::connect(&model, &HostListModel::hostOpened, &model,
                     [&](const QString& id, const QString&, const QString&, quint16 port) {
                       opened_id = id;
                       opened_port = port;
                     });
    failures += expect(model.openHost(0) && opened_id == primary_id && opened_port == 47984,
                       "primary instance must use its advertised HTTPS port");
    failures += expect(model.openHost(1) && opened_id == secondary_id && opened_port == 48084,
                       "secondary instance must use its own advertised HTTPS port");

    model.requirePairing(secondary_id);
    failures += expect(model.data(model.index(0), HostListModel::PairedRole).toBool() &&
                           !model.data(model.index(1), HostListModel::PairedRole).toBool(),
                       "authentication failure must unpair only the affected instance");
    failures += expect(model.data(model.index(1), HostListModel::OnlineRole).toBool() && !model.openHost(1),
                       "unpaired instances stay online but cannot open games");
    failures += expect(loadServerCertificate(secondary_id) == "secondary certificate",
                       "rejection must not replace or erase the pinned certificate");
    secondary.paired = true;
    model.handleServerInfo("192.0.2.1", 48089, secondary);
    failures += expect(!model.data(model.index(1), HostListModel::PairedRole).toBool(),
                       "unauthenticated serverinfo must not undo a certificate rejection");
    HostListModel restarted(nullptr, false);
    restarted.handleServerInfo("192.0.2.1", 48089, secondary);
    failures += expect(!restarted.data(restarted.index(0), HostListModel::PairedRole).toBool(),
                       "unpaired state must survive restarting the model");
    model.handlePairingCompleted(secondary_id, {.success = true, .server_certificate = "repaired certificate"});
    failures += expect(model.openHost(1) && !hostNeedsPairing(secondary_id) &&
                           loadServerCertificate(primary_id) == "primary certificate" &&
                           loadServerCertificate(secondary_id) == "repaired certificate",
                       "successful re-pairing must restore only that instance");
    model.handleServerInfo("192.0.2.2", 48089, secondary);
    failures += expect(model.rowCount() == 2 && model.data(model.index(1), HostListModel::AddressRole) == "192.0.2.2",
                       "an instance changing address must retain its identity");
    return failures;
  }
};

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  QCoreApplication::setOrganizationName("One tests");
  QCoreApplication::setApplicationName("Host list test");
  QTemporaryDir settings;
  if (!settings.isValid()) return 1;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  HostListModel model(nullptr, false);
  int failures = 0;

  failures += expect(model.rowCount() == 0, "host list should begin empty");
  failures += expect(!model.addManualHost("   "), "blank manual host should be rejected");
  failures += expect(!model.addManualHost("example.test", 65536), "invalid port should be rejected");
  failures += expect(model.addManualHost("  example.test  "), "valid manual host should be accepted");
  failures += expect(model.rowCount() == 1, "manual host should appear in the model");

  const auto first = model.index(0);
  failures += expect(model.data(first, HostListModel::AddressRole).toString() == QStringLiteral("example.test"),
                     "manual host address should be normalized");
  failures += expect(model.data(first, HostListModel::PortRole).toInt() == 47989,
                     "manual host should use the GameStream default port");
  failures +=
      expect(model.data(first, HostListModel::ManuallyAddedRole).toBool(), "manual host role should be exposed");

  failures += expect(model.addManualHost("example.test"), "existing manual host should be refreshable");
  failures += expect(model.rowCount() == 1, "refreshing a host must not duplicate it");
  failures += expect(model.addManualHost("example.test:48089") && model.rowCount() == 2,
                     "manual custom port must create a separate instance");
  failures += expect(model.data(model.index(1), HostListModel::AddressRole) == "example.test" &&
                         model.data(model.index(1), HostListModel::PortRole).toInt() == 48089,
                     "manual endpoint must separate hostname and port");
  failures +=
      expect(model.addManualHost("[::1]:48089") && model.data(model.index(2), HostListModel::AddressRole) == "::1" &&
                 model.data(model.index(2), HostListModel::PortRole).toInt() == 48089,
             "bracketed IPv6 custom port must be supported");
  failures += expect(model.addManualHost("::1") && model.data(model.index(3), HostListModel::PortRole).toInt() == 47989,
                     "bare IPv6 must keep the default port");
  for (const auto* address : {"example.test:65536", "example.test:0", "example.test:invalid", "[::1]:-1"}) {
    failures += expect(!model.addManualHost(address), "invalid explicit port must be rejected");
  }
  failures += HostListModelTest::instances();
  saveServerCertificate("legacy-uuid", "legacy certificate");
  setHostNeedsPairing("legacy-uuid", true);
  migrateLegacyHostCertificate("legacy-uuid", 48089);
  failures += expect(loadServerCertificate(hostInstanceId("legacy-uuid", 48089)).isEmpty(),
                     "custom-port instances must never inherit ambiguous legacy trust");
  migrateLegacyHostCertificate("legacy-uuid", 47989);
  failures += expect(loadServerCertificate(hostInstanceId("legacy-uuid", 47989)) == "legacy certificate" &&
                         hostNeedsPairing(hostInstanceId("legacy-uuid", 47989)),
                     "default-port legacy migration must retain both "
                     "certificate and rejection state");
  saveServerCertificate(hostInstanceId("legacy-uuid", 47989), "new certificate");
  migrateLegacyHostCertificate("legacy-uuid", 47989);
  failures += expect(loadServerCertificate(hostInstanceId("legacy-uuid", 47989)) == "new certificate",
                     "legacy migration must never overwrite newly paired trust");
  failures += HostListModelTest::persistence();
  return failures == 0 ? 0 : 1;
}
