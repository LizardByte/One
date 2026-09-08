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
#include "host_settings.hpp"
#include "launch_request.hpp"

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

  const auto closed_port = second.serverPort();
  second.close();
  failed.clear();
  unpaired.clear();
  client.query(second_id, "127.0.0.1", closed_port, server_two.certificatePem());
  failures += expect(until([&]() { return !failed.isEmpty(); }) && unpaired.isEmpty(),
                     "connection failure must not revoke a previously valid pairing");
  return failures == 0 ? 0 : 1;
}
