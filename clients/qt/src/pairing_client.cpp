#include "pairing_client.hpp"

#include <cctype>
#include <memory>
#include <stdexcept>

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QtConcurrentRun>
#include <QUrlQuery>
#include <QUuid>
#include <QXmlStreamReader>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include "client_identity.hpp"

namespace {

constexpr auto request_timeout_ms = 5000;

template <typename T, auto Deleter>
struct OpenSslDeleter {
  void operator()(T* value) const noexcept {
    if (value != nullptr) {
      static_cast<void>(Deleter(value));
    }
  }
};

template <typename T, auto Deleter>
using OpenSslPointer = std::unique_ptr<T, OpenSslDeleter<T, Deleter>>;

struct PairingCredentials {
  QString unique_id;
  QByteArray certificate_pem;
  QByteArray private_key_pem;
};

[[noreturn]] void fail(const QString& message) {
  throw std::runtime_error(message.toStdString());
}

QByteArray random_bytes(const int length) {
  QByteArray result(length, Qt::Uninitialized);
  if (RAND_bytes(reinterpret_cast<unsigned char*>(result.data()), length) != 1) {
    fail(QStringLiteral("Unable to generate cryptographically secure pairing data"));
  }
  return result;
}

QByteArray crypt_aes_ecb(const QByteArray& input, const QByteArray& key, const bool encrypt) {
  if (input.isEmpty() || input.size() % 16 != 0 || key.size() != 16) {
    fail(QStringLiteral("The pairing server returned invalid encrypted data"));
  }

  OpenSslPointer<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> context(EVP_CIPHER_CTX_new());
  if (!context || EVP_CipherInit_ex(
                      context.get(),
                      EVP_aes_128_ecb(),
                      nullptr,
                      reinterpret_cast<const unsigned char*>(key.constData()),
                      nullptr,
                      encrypt ? 1 : 0) != 1 ||
      EVP_CIPHER_CTX_set_padding(context.get(), 0) != 1) {
    fail(QStringLiteral("Unable to initialize pairing encryption"));
  }

  QByteArray output(input.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
  int output_length = 0;
  int final_length = 0;
  if (EVP_CipherUpdate(
          context.get(),
          reinterpret_cast<unsigned char*>(output.data()),
          &output_length,
          reinterpret_cast<const unsigned char*>(input.constData()),
          static_cast<int>(input.size())) != 1 ||
      EVP_CipherFinal_ex(
          context.get(),
          reinterpret_cast<unsigned char*>(output.data()) + output_length,
          &final_length) != 1) {
    fail(QStringLiteral("Pairing encryption failed"));
  }
  output.resize(output_length + final_length);
  return output;
}

OpenSslPointer<X509, X509_free> parse_certificate(const QByteArray& pem) {
  OpenSslPointer<BIO, BIO_free> bio(BIO_new_mem_buf(pem.constData(), static_cast<int>(pem.size())));
  if (!bio) {
    fail(QStringLiteral("Unable to allocate a certificate reader"));
  }

  auto* certificate = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
  if (certificate == nullptr) {
    fail(QStringLiteral("The pairing server returned an invalid certificate"));
  }
  return OpenSslPointer<X509, X509_free>(certificate);
}

QByteArray certificate_signature(X509* certificate) {
  const ASN1_BIT_STRING* signature = nullptr;
  X509_get0_signature(&signature, nullptr, certificate);
  if (signature == nullptr) {
    fail(QStringLiteral("The pairing certificate has no signature"));
  }
  return {
      reinterpret_cast<const char*>(ASN1_STRING_get0_data(signature)),
      ASN1_STRING_length(signature),
  };
}

QByteArray sign_message(const QByteArray& message, const QByteArray& private_key_pem) {
  OpenSslPointer<BIO, BIO_free> bio(
      BIO_new_mem_buf(private_key_pem.constData(), static_cast<int>(private_key_pem.size())));
  if (!bio) {
    fail(QStringLiteral("Unable to allocate a private-key reader"));
  }
  OpenSslPointer<EVP_PKEY, EVP_PKEY_free> private_key(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
  OpenSslPointer<EVP_MD_CTX, EVP_MD_CTX_free> context(EVP_MD_CTX_new());
  if (!private_key || !context ||
      EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, private_key.get()) != 1 ||
      EVP_DigestSignUpdate(context.get(), message.constData(), static_cast<std::size_t>(message.size())) != 1) {
    fail(QStringLiteral("Unable to initialize the client pairing signature"));
  }

  std::size_t signature_length = 0;
  if (EVP_DigestSignFinal(context.get(), nullptr, &signature_length) != 1) {
    fail(QStringLiteral("Unable to determine the client pairing signature size"));
  }
  QByteArray signature(static_cast<qsizetype>(signature_length), Qt::Uninitialized);
  if (EVP_DigestSignFinal(
          context.get(), reinterpret_cast<unsigned char*>(signature.data()), &signature_length) != 1) {
    fail(QStringLiteral("Unable to sign the client pairing secret"));
  }
  signature.resize(static_cast<qsizetype>(signature_length));
  return signature;
}

bool verify_signature(
    const QByteArray& message,
    const QByteArray& signature,
    X509* server_certificate) {
  OpenSslPointer<EVP_PKEY, EVP_PKEY_free> public_key(X509_get_pubkey(server_certificate));
  OpenSslPointer<EVP_MD_CTX, EVP_MD_CTX_free> context(EVP_MD_CTX_new());
  if (!public_key || !context ||
      EVP_DigestVerifyInit(context.get(), nullptr, EVP_sha256(), nullptr, public_key.get()) != 1 ||
      EVP_DigestVerifyUpdate(context.get(), message.constData(), static_cast<std::size_t>(message.size())) != 1) {
    fail(QStringLiteral("Unable to initialize server signature verification"));
  }
  return EVP_DigestVerifyFinal(
             context.get(),
             reinterpret_cast<const unsigned char*>(signature.constData()),
             static_cast<std::size_t>(signature.size())) == 1;
}

QHash<QString, QString> parse_response(const QByteArray& xml) {
  QXmlStreamReader reader(xml);
  QHash<QString, QString> values;
  int status_code = 0;
  QString status_message;

  while (!reader.atEnd()) {
    if (reader.readNext() != QXmlStreamReader::StartElement) {
      continue;
    }
    if (reader.name() == u"root") {
      status_code = reader.attributes().value(QStringLiteral("status_code")).toInt();
      status_message = reader.attributes().value(QStringLiteral("status_message")).toString();
    } else {
      values.insert(reader.name().toString(), reader.readElementText());
    }
  }

  if (reader.hasError()) {
    fail(reader.errorString());
  }
  if (status_code != 200) {
    fail(status_message.isEmpty() ? QStringLiteral("Pairing server returned status %1").arg(status_code)
                                  : status_message);
  }
  return values;
}

QByteArray hex_value(const QHash<QString, QString>& values, const QString& name) {
  const auto text = values.value(name).toLatin1();
  if (text.isEmpty() || text.size() % 2 != 0) {
    fail(QStringLiteral("Pairing response is missing %1").arg(name));
  }
  for (const auto character : text) {
    if (!std::isxdigit(static_cast<unsigned char>(character))) {
      fail(QStringLiteral("Pairing response contains invalid %1 data").arg(name));
    }
  }
  return QByteArray::fromHex(text);
}

QByteArray request(
    const PairingTarget& target,
    const PairingCredentials& credentials,
    const QString& path,
    const bool secure,
    const QList<QPair<QString, QString>>& query_items,
    const int timeout_ms = request_timeout_ms,
    const QSslCertificate* pinned_certificate = nullptr) {
  if (secure && (pinned_certificate == nullptr || pinned_certificate->isNull())) {
    fail(QStringLiteral("A trusted server certificate is required for secure pairing"));
  }
  QUrl url;
  url.setScheme(secure ? QStringLiteral("https") : QStringLiteral("http"));
  url.setHost(target.address);
  url.setPort(secure ? target.https_port : target.http_port);
  url.setPath(path);

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("uniqueid"), credentials.unique_id);
  query.addQueryItem(QStringLiteral("uuid"), QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));
  for (const auto& [name, value] : query_items) {
    query.addQueryItem(name, value);
  }
  url.setQuery(query);

  QNetworkRequest network_request(url);
  network_request.setTransferTimeout(timeout_ms);
  network_request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  network_request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
  if (secure) {
    auto configuration = QSslConfiguration::defaultConfiguration();
    configuration.setLocalCertificate(QSslCertificate(credentials.certificate_pem));
    configuration.setPrivateKey(QSslKey(credentials.private_key_pem, QSsl::Rsa));
    network_request.setSslConfiguration(configuration);
  }

  QNetworkAccessManager network;
  network.setProxy(QNetworkProxy::NoProxy);
  auto* reply = network.get(network_request);
  if (secure) {
    const auto trusted_certificate = *pinned_certificate;
    QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply, trusted_certificate](const QList<QSslError>& errors) {
      if (errors.isEmpty()) {
        return;
      }
      for (const auto& error : errors) {
        if (error.certificate() != trusted_certificate) {
          return;
        }
      }
      reply->ignoreSslErrors(errors);
    });
  }

  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  if (QCoreApplication::instance() != nullptr) {
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, reply, &QNetworkReply::abort);
  }
  loop.exec();

  if (reply->error() != QNetworkReply::NoError) {
    fail(reply->errorString());
  }
  if (secure && reply->sslConfiguration().peerCertificate() != *pinned_certificate) {
    fail(QStringLiteral("The pairing server's TLS certificate changed during pairing"));
  }
  return reply->readAll();
}

QList<QPair<QString, QString>> pair_query(const QString& name, const QString& value) {
  return {
      {QStringLiteral("devicename"), QStringLiteral("roth")},
      {QStringLiteral("updateState"), QStringLiteral("1")},
      {name, value},
  };
}

void require_paired(const QHash<QString, QString>& response, const QString& stage) {
  if (response.value(QStringLiteral("paired")) != QStringLiteral("1")) {
    fail(QStringLiteral("Pairing failed at %1").arg(stage));
  }
}

void try_unpair(const PairingTarget& target, const PairingCredentials& credentials) noexcept {
  try {
    static_cast<void>(request(
        target,
        credentials,
        QStringLiteral("/unpair"),
        false,
        {}));
  } catch (...) {
  }
}

PairingResult perform_pairing(
    const PairingTarget& target,
    const PairingCredentials& credentials,
    const QString& pin) {
  bool pairing_started = false;
  try {
    bool version_valid = false;
    const auto server_major_version = target.app_version.section(u'.', 0, 0).toInt(&version_valid);
    if (!version_valid) {
      fail(QStringLiteral("The server reported an invalid application version"));
    }
    const auto hash_algorithm = server_major_version >= 7 ? QCryptographicHash::Sha256 : QCryptographicHash::Sha1;
    const auto hash_length = QCryptographicHash::hashLength(hash_algorithm);

    const auto salt = random_bytes(16);
    auto aes_key = QCryptographicHash::hash(salt + pin.toUtf8(), hash_algorithm);
    aes_key.truncate(16);

    pairing_started = true;
    auto response = parse_response(request(
        target,
        credentials,
        QStringLiteral("/pair"),
        false,
        {
            {QStringLiteral("devicename"), QStringLiteral("roth")},
            {QStringLiteral("updateState"), QStringLiteral("1")},
            {QStringLiteral("phrase"), QStringLiteral("getservercert")},
            {QStringLiteral("salt"), QString::fromLatin1(salt.toHex())},
            {QStringLiteral("clientcert"), QString::fromLatin1(credentials.certificate_pem.toHex())},
        },
        0));
    require_paired(response, QStringLiteral("certificate exchange"));
    const auto server_certificate_pem = hex_value(response, QStringLiteral("plaincert"));
    const QSslCertificate pinned_certificate(server_certificate_pem);
    if (pinned_certificate.isNull()) {
      fail(QStringLiteral("The pairing server returned an invalid certificate"));
    }
    auto server_certificate = parse_certificate(server_certificate_pem);
    auto client_certificate = parse_certificate(credentials.certificate_pem);

    const auto random_challenge = random_bytes(16);
    const auto encrypted_challenge = crypt_aes_ecb(random_challenge, aes_key, true);
    response = parse_response(request(
        target,
        credentials,
        QStringLiteral("/pair"),
        false,
        pair_query(QStringLiteral("clientchallenge"), QString::fromLatin1(encrypted_challenge.toHex()))));
    require_paired(response, QStringLiteral("challenge exchange"));

    const auto challenge_response = crypt_aes_ecb(
        hex_value(response, QStringLiteral("challengeresponse")), aes_key, false);
    if (challenge_response.size() != hash_length + 16) {
      fail(QStringLiteral("The pairing server returned an invalid challenge response"));
    }
    const auto server_response = challenge_response.left(hash_length);
    const auto client_secret = random_bytes(16);

    QByteArray response_data = challenge_response.mid(hash_length, 16);
    response_data.append(certificate_signature(client_certificate.get()));
    response_data.append(client_secret);
    auto padded_hash = QCryptographicHash::hash(response_data, hash_algorithm);
    padded_hash.resize(32);

    const auto encrypted_response = crypt_aes_ecb(padded_hash, aes_key, true);
    response = parse_response(request(
        target,
        credentials,
        QStringLiteral("/pair"),
        false,
        pair_query(QStringLiteral("serverchallengeresp"), QString::fromLatin1(encrypted_response.toHex()))));
    require_paired(response, QStringLiteral("challenge verification"));

    const auto pairing_secret = hex_value(response, QStringLiteral("pairingsecret"));
    if (pairing_secret.size() <= 16) {
      fail(QStringLiteral("The pairing server returned an invalid pairing secret"));
    }
    const auto server_secret = pairing_secret.left(16);
    const auto server_signature = pairing_secret.mid(16);
    if (!verify_signature(server_secret, server_signature, server_certificate.get())) {
      fail(QStringLiteral("The pairing server failed certificate verification"));
    }

    QByteArray expected_response = random_challenge;
    expected_response.append(certificate_signature(server_certificate.get()));
    expected_response.append(server_secret);
    if (QCryptographicHash::hash(expected_response, hash_algorithm) != server_response) {
      fail(QStringLiteral("The PIN was rejected"));
    }

    QByteArray client_pairing_secret = client_secret;
    client_pairing_secret.append(sign_message(client_secret, credentials.private_key_pem));
    response = parse_response(request(
        target,
        credentials,
        QStringLiteral("/pair"),
        false,
        pair_query(QStringLiteral("clientpairingsecret"), QString::fromLatin1(client_pairing_secret.toHex()))));
    require_paired(response, QStringLiteral("client authentication"));

    response = parse_response(request(
        target,
        credentials,
        QStringLiteral("/pair"),
        true,
        pair_query(QStringLiteral("phrase"), QStringLiteral("pairchallenge")),
        request_timeout_ms,
        &pinned_certificate));
    require_paired(response, QStringLiteral("secure confirmation"));
    return {
        .success = true,
        .server_certificate = server_certificate_pem,
    };
  } catch (const std::exception& error) {
    if (pairing_started) {
      try_unpair(target, credentials);
    }
    return {
        .success = false,
        .error = QString::fromUtf8(error.what()),
    };
  }
}

}  // namespace

PairingClient::PairingClient(const ClientIdentity& identity, QObject* parent)
    : QObject(parent),
      unique_id_(identity.uniqueId()),
      certificate_pem_(identity.certificatePem()),
      private_key_pem_(identity.privateKeyPem()) {}

QString PairingClient::start(const PairingTarget& target) {
  if (target.stable_id.isEmpty() || target.address.isEmpty() || target.app_version.isEmpty() ||
      target.http_port == 0 || target.https_port == 0 || active_hosts_.contains(target.stable_id)) {
    return {};
  }

  const auto pin = generatePin();
  active_hosts_.insert(target.stable_id);
  const PairingCredentials credentials {
      .unique_id = unique_id_,
      .certificate_pem = certificate_pem_,
      .private_key_pem = private_key_pem_,
  };

  auto* watcher = new QFutureWatcher<PairingResult>(this);
  connect(watcher, &QFutureWatcher<PairingResult>::finished, this, [this, watcher, stable_id = target.stable_id]() {
    active_hosts_.remove(stable_id);
    const auto result = watcher->result();
    watcher->deleteLater();
    emit completed(stable_id, result);
  });
  watcher->setFuture(QtConcurrent::run([target, credentials, pin]() {
    return perform_pairing(target, credentials, pin);
  }));
  return pin;
}

QString PairingClient::generatePin() {
  return QStringLiteral("%1").arg(QRandomGenerator::system()->bounded(10000), 4, 10, QLatin1Char('0'));
}
