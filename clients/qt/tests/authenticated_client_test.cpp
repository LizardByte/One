#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSettings>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>

#include "application_list_client.hpp"
#include "application_list_model.hpp"
#include "client_identity.hpp"
#include "host_list_model.hpp"
#include "host_settings.hpp"
#include "launch_request.hpp"
#include "server_info_client.hpp"

// Exercise the production polling setup without starting LAN mDNS discovery.
class HostListModelTest {
 public:
  static std::unique_ptr<HostListModel> pollingModel() {
    auto model = std::make_unique<HostListModel>(nullptr, false);
    model->identity_ = std::make_unique<ClientIdentity>();
    model->server_info_ = std::make_unique<ServerInfoClient>(*model->identity_);
    model->startPolling();
    return model;
  }
};

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << message << '\n';
  return 1;
}

template <typename Predicate>
bool until(Predicate predicate) {
  QElapsedTimer timer;
  timer.start();
  while (!predicate() && timer.elapsed() < 7000) QTest::qWait(10);
  return predicate();
}

// Two independent TLS endpoints on loopback exercise the actual Qt network
// clients. Certificates and client identity stay in temporary test settings.
class TlsHost final : public QTcpServer {
 public:
  explicit TlsHost(const ClientIdentity& identity) : ssl_(identity.sslConfiguration()) {
    ssl_.setPeerVerifyMode(QSslSocket::QueryPeer);
  }

  QByteArray body{
      "<root "
      "status_code=\"200\"><App><ID>1</ID><AppTitle>Desktop</AppTitle></App></"
      "root>"};
  int status{200};
  int delay_ms{0};
  int requests{0};
  QSslCertificate client_certificate;
  QByteArray last_request;

 protected:
  void incomingConnection(qintptr descriptor) override {
    auto* socket = new QSslSocket(this);
    socket->setSslConfiguration(ssl_);
    if (!socket->setSocketDescriptor(descriptor)) {
      socket->deleteLater();
      return;
    }
    connect(socket, &QSslSocket::sslErrors, socket, [socket](const QList<QSslError>& errors) {
      // The mock Sunshine server requests a client certificate, and
      // accepts self-signed test identities just as a successfully
      // paired server does.
      socket->ignoreSslErrors(errors);
    });
    connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QSslSocket::readyRead, this, [this, socket, buffer = QByteArray()]() mutable {
      buffer += socket->readAll();
      if (!buffer.contains("\r\n\r\n") || socket->property("responded").toBool()) return;
      socket->setProperty("responded", true);
      ++requests;
      client_certificate = socket->peerCertificate();
      last_request = buffer;
      const auto response =
          "HTTP/1.1 " + QByteArray::number(status) +
          " Test\r\nContent-Type: application/xml\r\nContent-Length: " + QByteArray::number(body.size()) +
          "\r\nConnection: close\r\n\r\n" + body;
      QTimer::singleShot(delay_ms, socket, [socket, response]() {
        socket->write(response);
        socket->disconnectFromHost();
      });
    });
    socket->startServerEncryption();
  }

 private:
  QSslConfiguration ssl_;
};

class HttpHost final : public QTcpServer {
 public:
  explicit HttpHost(quint16 https_port) {
    body =
        "<root status_code=\"200\"><uniqueid>poll-uuid</uniqueid><hostname>Polling PC</hostname>"
        "<HttpsPort>" +
        QByteArray::number(https_port) + "</HttpsPort><PairStatus>0</PairStatus></root>";
    connect(this, &QTcpServer::newConnection, this, [this]() {
      while (auto* socket = nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer = QByteArray()]() mutable {
          buffer += socket->readAll();
          if (!buffer.contains("\r\n\r\n") || socket->property("responded").toBool()) return;
          socket->setProperty("responded", true);
          ++requests;
          socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                        "\r\nConnection: close\r\n\r\n" + body);
          socket->disconnectFromHost();
        });
      }
    });
  }
  QByteArray body;
  int requests{0};
};

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setOrganizationName("One tests");
  QTemporaryDir settings;
  if (!settings.isValid()) return 1;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  QCoreApplication::setApplicationName("TLS server one");
  const ClientIdentity server_one;
  QCoreApplication::setApplicationName("TLS server two");
  const ClientIdentity server_two;
  QCoreApplication::setApplicationName("Authenticated client test");
  const ClientIdentity client_identity;
  TlsHost first(server_one);
  TlsHost second(server_two);
  if (!first.listen(QHostAddress::LocalHost) || !second.listen(QHostAddress::LocalHost)) return 1;

  const auto first_id = hostInstanceId("same-uuid", first.serverPort());
  const auto second_id = hostInstanceId("same-uuid", second.serverPort());
  const QByteArray rejected =
      "<root status_code=\"401\" status_message=\"The client is not "
      "authorized. Certificate verification failed.\"/>";
  const auto success_body = first.body;
  const auto cover_png = QByteArray::fromBase64(
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aWQAAAABJRU5ErkJggg==");
  int failures = 0;
  ApplicationListClient client(client_identity);
  QSignalSpy succeeded(&client, &ApplicationListClient::succeeded);
  QSignalSpy failed(&client, &ApplicationListClient::failed);
  QSignalSpy unpaired(&client, &ApplicationListClient::authenticationRequired);
  auto query_first = [&](const QByteArray& certificate) {
    succeeded.clear();
    failed.clear();
    unpaired.clear();
    client.query(first_id, "127.0.0.1", first.serverPort(), certificate);
    return until([&]() { return !succeeded.isEmpty() || !failed.isEmpty(); });
  };

  client.query(first_id, "127.0.0.1", first.serverPort(), server_one.certificatePem());
  client.query(second_id, "127.0.0.1", second.serverPort(), server_two.certificatePem());
  failures += expect(until([&]() { return succeeded.size() + failed.size() == 2; }) && succeeded.size() == 2 &&
                         unpaired.isEmpty() && succeeded[0][0] != succeeded[1][0],
                     "concurrent requests to two instances must not share pending "
                     "state or certificates");
  failures += expect(first.client_certificate == QSslCertificate(client_identity.certificatePem()) &&
                         second.client_certificate == first.client_certificate,
                     "both instances must receive the persisted client certificate");
  failures += expect(first.last_request.startsWith("GET /applist?"), "test must exercise the real applist endpoint");

  first.body = rejected;
  failures += expect(query_first(server_one.certificatePem()) && failed.size() == 1 && unpaired.size() == 1 &&
                         unpaired[0][0] == first_id && succeeded.isEmpty(),
                     "Sunshine XML 401 must require pairing for the rejected instance");
  first.status = 401;
  failures += expect(query_first(server_one.certificatePem()) && failed.size() == 1 && unpaired.size() == 1,
                     "HTTP 401 from the pinned peer must also require pairing");
  first.status = 200;
  first.body = success_body;
  failures += expect(
      query_first(server_two.certificatePem()) && failed.size() == 1 && unpaired.size() == 1 && succeeded.isEmpty(),
      "a different TLS certificate must not return a library and must "
      "require pairing");
  failures += expect(query_first({}) && failed.size() == 1 && unpaired.size() == 1,
                     "missing certificate must require pairing without sending a request");
  first.body = "<root status_code=\"500\" status_message=\"Temporary failure\"/>";
  failures += expect(query_first(server_one.certificatePem()) && failed.size() == 1 && unpaired.isEmpty(),
                     "ordinary server errors must not unpair an instance");
  first.body = "<root status_code=\"401\">";
  failures += expect(query_first(server_one.certificatePem()) && failed.size() == 1 && unpaired.isEmpty(),
                     "malformed XML must not be classified as an authentication rejection");
  first.body = success_body;

  // Library state must clear cached games and remain recoverable through
  // pairing.
  saveServerCertificate(first_id, server_one.certificatePem());
  saveServerCertificate(second_id, server_two.certificatePem());
  ApplicationListModel model;
  QSignalSpy model_unpaired(&model, &ApplicationListModel::hostAuthenticationFailed);
  model.loadHost(first_id, "First", "127.0.0.1", first.serverPort(), {}, {}, 1, 0);
  failures += expect(until([&]() { return !model.loading(); }) && model.rowCount() == 1 && !model.pairingRequired(),
                     "valid pairing must load the application model");
  first.body = cover_png;
  model.requestArtwork(1);
  failures += expect(
      until([&]() { return !model.data(model.index(0), ApplicationListModel::ArtworkRole).toString().isEmpty(); }),
      "authenticated artwork must populate the matching application role");
  const auto artwork_requests = first.requests;
  model.requestArtwork(1);
  QTest::qWait(50);
  failures += expect(first.requests == artwork_requests, "visible cards must reuse the per-library artwork cache");
  first.body = success_body;
  model.refresh();
  failures += expect(until([&]() { return !model.loading(); }), "refresh must allow new artwork requests");
  first.body.clear();
  model.requestArtwork(1);
  failures += expect(until([&]() {
                       return !model.data(model.index(0), ApplicationListModel::ArtworkErrorRole).toString().isEmpty();
                     }) &&
                         !model.pairingRequired(),
                     "empty artwork must expose a retryable error without unpairing");
  first.body = cover_png;
  model.retryArtwork(1);
  model.retryArtwork(1);
  const auto retry_start = first.requests;
  failures += expect(
      until([&]() { return !model.data(model.index(0), ApplicationListModel::ArtworkRole).toString().isEmpty(); }) &&
          model.data(model.index(0), ApplicationListModel::ArtworkErrorRole).toString().isEmpty() &&
          first.requests == retry_start + 1,
      "retry must load the cover, clear its error and coalesce repeated clicks");
  first.body = rejected;
  model.refresh();
  failures += expect(until([&]() { return !model.loading(); }) && model.pairingRequired() && model.rowCount() == 0 &&
                         model_unpaired.size() == 1 && model_unpaired[0][0] == first_id,
                     "authentication rejection must clear stale games and expose "
                     "unpaired UI state");
  const auto rejected_requests = first.requests;
  model.refresh();
  QTest::qWait(50);
  failures += expect(!model.loading() && first.requests == rejected_requests,
                     "refresh must not loop on a known unpaired instance");
  first.body = success_body;
  saveServerCertificate(first_id, server_one.certificatePem());
  model.loadHost(first_id, "First", "127.0.0.1", first.serverPort(), {}, {}, 1, 0);
  failures += expect(until([&]() { return !model.loading(); }) && model.rowCount() == 1 && !model.pairingRequired(),
                     "opening an instance after re-pairing must restore the library");

  // Switching while the previous instance is still responding must not stall
  // the new library, or lose the old instance's authentication failure.
  first.delay_ms = 150;
  first.body = rejected;
  model.refresh();
  model.loadHost(second_id, "Second", "127.0.0.1", second.serverPort(), {}, {}, 1, 0);
  failures += expect(until([&]() { return model_unpaired.size() == 2 && !model.loading(); }) && model.rowCount() == 1 &&
                         !model.pairingRequired() && model.hostName() == "Second" && model_unpaired[1][0] == first_id,
                     "late rejection must target the old instance without disturbing the "
                     "selected library");
  first.delay_ms = 0;

  LaunchClient launch(client_identity);
  QObject::connect(&launch, &LaunchClient::authenticationRequired, &model, &ApplicationListModel::requirePairing);
  QSignalSpy launch_failed(&launch, &LaunchClient::failed);
  QSignalSpy launch_succeeded(&launch, &LaunchClient::succeeded);
  QSignalSpy launch_unpaired(&launch, &LaunchClient::authenticationRequired);
  auto send_launch = [&](LaunchAction action, const QByteArray& certificate) {
    launch_failed.clear();
    launch_succeeded.clear();
    launch_unpaired.clear();
    launch.send(first_id, "127.0.0.1", first.serverPort(), certificate, action, {}, {.application_id = 1});
    return until([&]() { return !launch_failed.isEmpty() || !launch_succeeded.isEmpty(); });
  };
  for (const auto action : {LaunchAction::launch, LaunchAction::resume}) {
    failures += expect(send_launch(action, server_one.certificatePem()) && launch_failed.size() == 1 &&
                           launch_unpaired.size() == 1 && launch_unpaired[0][0] == first_id,
                       "launch and resume must invalidate only the rejected instance's "
                       "pairing");
  }
  failures += expect(!model.pairingRequired() && model.rowCount() == 1,
                     "launch rejection from another instance must not clear the selected library");
  second.body = rejected;
  launch.send(second_id, "127.0.0.1", second.serverPort(), server_two.certificatePem(), LaunchAction::launch, {},
              {.application_id = 1});
  failures +=
      expect(until([&]() { return model.pairingRequired(); }) && model.rowCount() == 0 && !model.error().isEmpty(),
             "launch rejection must also clear the affected library and show pairing recovery");
  first.body =
      "<root "
      "status_code=\"200\"><sessionUrl0>rtsp://127.0.0.1:48010</sessionUrl0></"
      "root>";
  failures += expect(send_launch(LaunchAction::launch, server_one.certificatePem()) && launch_succeeded.size() == 1 &&
                         launch_unpaired.isEmpty(),
                     "a pinned authorized instance must still launch successfully");
  failures += expect(send_launch(LaunchAction::launch, server_two.certificatePem()) && launch_failed.size() == 1 &&
                         launch_unpaired.size() == 1,
                     "launch must require pairing on a server certificate mismatch");
  first.body = rejected;
  launch_unpaired.clear();
  launch.cancel(first_id, "127.0.0.1", first.serverPort(), server_one.certificatePem());
  failures += expect(until([&]() { return launch_unpaired.size() == 1; }) && launch_unpaired[0][0] == first_id,
                     "cancel rejection must invalidate the correct pairing");
  launch_unpaired.clear();
  launch.cancel(first_id, "127.0.0.1", first.serverPort(), server_two.certificatePem());
  failures +=
      expect(until([&]() { return launch_unpaired.size() == 1; }), "cancel certificate mismatch must require pairing");

  QSignalSpy artwork(&client, &ApplicationListClient::artworkReady);
  failed.clear();
  unpaired.clear();
  first.body = cover_png;
  second.body = cover_png;
  client.query(first_id, "127.0.0.1", first.serverPort(), server_one.certificatePem(), 1);
  client.query(second_id, "127.0.0.1", second.serverPort(), server_two.certificatePem(), 1);
  failures += expect(until([&]() { return artwork.size() == 2; }) && artwork[0][0] != artwork[1][0] &&
                         artwork[0][1].toInt() == 1 && artwork[1][1].toInt() == 1 &&
                         artwork[0][2].toString().startsWith("data:image/png;base64,") &&
                         artwork[1][2].toString().startsWith("data:image/png;base64,"),
                     "same application ID on two instances must fetch independent authenticated artwork");
  failures += expect(first.last_request.startsWith("GET /appasset?") && first.last_request.contains("appid=1"),
                     "artwork must use Sunshine's application asset endpoint");
  artwork.clear();
  first.body = rejected;
  client.query(first_id, "127.0.0.1", first.serverPort(), server_one.certificatePem(), 1);
  failures += expect(until([&]() { return artwork.size() == 1; }) && artwork[0][2].toString().isEmpty() &&
                         unpaired.size() == 1 && failed.isEmpty(),
                     "artwork authorization rejection must require pairing without becoming a generic library failure");
  artwork.clear();
  unpaired.clear();
  first.status = 404;
  client.query(first_id, "127.0.0.1", first.serverPort(), server_one.certificatePem(), 1);
  failures += expect(until([&]() { return artwork.size() == 1; }) && artwork[0][2].toString().isEmpty() &&
                         unpaired.isEmpty() && failed.isEmpty(),
                     "missing cover images must retain the library and pairing");
  first.status = 200;
  first.body = cover_png;
  artwork.clear();
  client.query(first_id, "127.0.0.1", first.serverPort(), server_two.certificatePem(), 1);
  failures +=
      expect(until([&]() { return artwork.size() == 1; }) && artwork[0][2].toString().isEmpty() && unpaired.size() == 1,
             "artwork must enforce the same server certificate pin as the application list");
  artwork.clear();
  unpaired.clear();
  first.body = cover_png + QByteArray(4 * 1024 * 1024, 'x');
  client.query(first_id, "127.0.0.1", first.serverPort(), server_one.certificatePem(), 1);
  failures +=
      expect(until([&]() { return artwork.size() == 1; }) && artwork[0][2].toString().isEmpty() && unpaired.isEmpty(),
             "oversized cover responses must be discarded without revoking pairing");

  {
    HttpHost first_http(first.serverPort());
    HttpHost second_http(second.serverPort());
    if (!first_http.listen(QHostAddress::LocalHost) || !second_http.listen(QHostAddress::LocalHost)) return 1;
    const auto first_poll_id = hostInstanceId("poll-uuid", first_http.serverPort());
    const auto second_poll_id = hostInstanceId("poll-uuid", second_http.serverPort());
    saveServerCertificate(first_poll_id, server_one.certificatePem());
    saveServerCertificate(second_poll_id, server_two.certificatePem());
    auto authorized = [](const HttpHost& host) {
      auto body = host.body;
      return body.replace("<PairStatus>0", "<PairStatus>1");
    };
    first.body = authorized(first_http);
    second.body = authorized(second_http);
    auto model = HostListModelTest::pollingModel();
    auto paired = [&](int row) { return model->data(model->index(row), HostListModel::PairedRole).toBool(); };
    auto online = [&](int row) { return model->data(model->index(row), HostListModel::OnlineRole).toBool(); };
    auto* timer = model->findChild<QTimer*>("hostRefreshTimer");
    failures += expect(timer && timer->isActive() && timer->interval() == 3000,
                       "host page must schedule polling every three seconds");
    model->addManualHost("127.0.0.1", first_http.serverPort());
    model->addManualHost("127.0.0.1", second_http.serverPort());
    failures += expect(until([&]() { return online(0) && online(1) && paired(0) && paired(1); }),
                       "host refresh must authenticate two separate instances despite plaintext PairStatus zero");
    failures += expect(first.last_request.startsWith("GET /serverinfo?") &&
                           first.client_certificate == QSslCertificate(client_identity.certificatePem()),
                       "host page polls must send the client identity to authenticated serverinfo");

    first.body = rejected;
    model->refresh();
    model->refresh();
    failures += expect(until([&]() { return !paired(0); }) && online(0) && paired(1) &&
                           hostNeedsPairing(first_poll_id) && !hostNeedsPairing(second_poll_id) &&
                           loadServerCertificate(first_poll_id) == server_one.certificatePem(),
                       "manual Refresh must unpair only the removed client and retain its server certificate");

    first.body = authorized(first_http);
    failures += expect(until([&]() { return paired(0); }) && !hostNeedsPairing(first_poll_id),
                       "automatic polling must restore authorization when a trusted client is re-enabled");
    first.status = 403;
    failures += expect(until([&]() { return !paired(0); }) && online(0) && paired(1),
                       "automatic polling must detect disabled clients without opening games");
    first.status = 200;
    model->refresh();
    failures += expect(until([&]() { return paired(0); }), "manual Refresh must also restore a re-enabled client");
    timer->stop();

    QSignalSpy changes(model.get(), &HostListModel::dataChanged);
    auto refresh_first = [&]() {
      changes.clear();
      model->refresh();
      return until([&]() {
        for (const auto& change : changes)
          if (change[0].value<QModelIndex>().row() == 0) return true;
        return false;
      });
    };
    first.body = "<root status_code=\"500\" status_message=\"Temporary failure\"/>";
    failures += expect(refresh_first() && paired(0) && online(0), "temporary errors must not unpair a host");
    first.body = "<root status_code=\"401\">";
    failures += expect(refresh_first() && paired(0), "malformed rejection XML must not revoke pairing");
    first.body = authorized(first_http).replace("<PairStatus>1</PairStatus>", "");
    failures += expect(refresh_first() && paired(0), "missing pairing status must not revoke pairing");
    first.body = first_http.body;
    failures +=
        expect(refresh_first() && !paired(0) && online(0), "authenticated PairStatus zero must unpair the client");
    first_http.body.replace("<PairStatus>0", "<PairStatus>1");
    first.body = "<root status_code=\"500\"/>";
    failures += expect(refresh_first() && !paired(0), "plaintext PairStatus must never restore rejected trust");
    first.body = authorized(first_http);
    failures += expect(refresh_first() && paired(0), "authenticated status must restore pairing");

    saveServerCertificate(first_poll_id, server_two.certificatePem());
    failures += expect(refresh_first() && !paired(0) && paired(1) && online(0),
                       "host refresh must reject a mismatched server certificate only for that instance");
    saveServerCertificate(first_poll_id, server_one.certificatePem());
    failures += expect(refresh_first() && paired(0), "restoring the trusted certificate must allow authentication");

    ServerInfoClient probe(client_identity);
    QSignalSpy probe_status(&probe, &ServerInfoClient::pairingStatus);
    QSignalSpy probe_done(&probe, &ServerInfoClient::succeeded);
    first.body = rejected;
    first.delay_ms = 150;
    const auto previous_requests = first.requests;
    probe.query("127.0.0.1", first_http.serverPort());
    failures += expect(until([&]() { return first.requests > previous_requests; }), "delayed poll must reach TLS");
    probe.cancel("127.0.0.1", first_http.serverPort());
    first.body = authorized(first_http);
    first.delay_ms = 0;
    probe.query("127.0.0.1", first_http.serverPort());
    failures += expect(until([&]() { return !probe_done.isEmpty(); }), "a new poll must proceed after cancellation");
    QTest::qWait(200);
    failures += expect(probe_done.size() == 1 && probe_status.size() == 1 && probe_status[0][1].toBool(),
                       "a canceled pre-pairing poll must not emit a stale authorization rejection");
    first.close();
    failures += expect(refresh_first() && paired(0) && online(0),
                       "a refused HTTPS connection must preserve pairing when HTTP remains reachable");
    first_http.close();
    failures += expect(refresh_first() && paired(0) && !online(0),
                       "an unreachable host must become offline without losing its pairing");
  }

  const auto closed_port = second.serverPort();
  second.close();
  failed.clear();
  unpaired.clear();
  client.query(second_id, "127.0.0.1", closed_port, server_two.certificatePem());
  failures += expect(until([&]() { return !failed.isEmpty(); }) && unpaired.isEmpty(),
                     "connection failure must not revoke a previously valid pairing");
  return failures == 0 ? 0 : 1;
}
