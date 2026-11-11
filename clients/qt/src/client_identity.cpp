#include "client_identity.hpp"

#include <memory>
#include <stdexcept>

#include <QSettings>
#include <QSslCertificate>
#include <QSslKey>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

namespace {

constexpr auto unique_id_key = "identity/uniqueId";
constexpr auto certificate_key = "identity/certificate";
constexpr auto private_key_key = "identity/privateKey";

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

QByteArray bio_bytes(BIO* bio) {
  BUF_MEM* memory = nullptr;
  BIO_get_mem_ptr(bio, &memory);
  if (memory == nullptr || memory->data == nullptr) {
    throw std::runtime_error("OpenSSL produced an empty memory buffer");
  }
  return {memory->data, static_cast<qsizetype>(memory->length)};
}

OpenSslPointer<EVP_PKEY, EVP_PKEY_free> generate_private_key() {
  OpenSslPointer<EVP_PKEY_CTX, EVP_PKEY_CTX_free> context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
  if (!context || EVP_PKEY_keygen_init(context.get()) <= 0 ||
      EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 2048) <= 0) {
    throw std::runtime_error("Unable to initialize the client RSA key generator");
  }

  EVP_PKEY* raw_key = nullptr;
  if (EVP_PKEY_keygen(context.get(), &raw_key) <= 0 || raw_key == nullptr) {
    throw std::runtime_error("Unable to generate the client RSA key");
  }
  return OpenSslPointer<EVP_PKEY, EVP_PKEY_free>(raw_key);
}

}  // namespace

ClientIdentity::ClientIdentity() {
  loadOrCreate();
}

const QString& ClientIdentity::uniqueId() const noexcept {
  return unique_id_;
}

const QByteArray& ClientIdentity::certificatePem() const noexcept {
  return certificate_pem_;
}

const QByteArray& ClientIdentity::privateKeyPem() const noexcept {
  return private_key_pem_;
}

QSslConfiguration ClientIdentity::sslConfiguration() const {
  auto configuration = QSslConfiguration::defaultConfiguration();
  configuration.setLocalCertificate(QSslCertificate(certificate_pem_));
  configuration.setPrivateKey(QSslKey(private_key_pem_, QSsl::Rsa));
  return configuration;
}

void ClientIdentity::createCredentials() {
  auto private_key = generate_private_key();
  OpenSslPointer<X509, X509_free> certificate(X509_new());
  if (!certificate) {
    throw std::runtime_error("Unable to allocate the client certificate");
  }

  X509_set_version(certificate.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 0);
  X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0);
  X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 60L * 60L * 24L * 365L * 20L);
  if (X509_set_pubkey(certificate.get(), private_key.get()) != 1) {
    throw std::runtime_error("Unable to assign the client certificate key");
  }

  OpenSslPointer<X509_NAME, X509_NAME_free> name(X509_NAME_new());
  constexpr auto common_name = "NVIDIA GameStream Client";
  if (!name || X509_NAME_add_entry_by_txt(
                   name.get(),
                   "CN",
                   MBSTRING_ASC,
                   reinterpret_cast<const unsigned char*>(common_name),
                   -1,
                   -1,
                   0) != 1 ||
      X509_set_subject_name(certificate.get(), name.get()) != 1 ||
      X509_set_issuer_name(certificate.get(), name.get()) != 1 ||
      X509_sign(certificate.get(), private_key.get(), EVP_sha256()) <= 0) {
    throw std::runtime_error("Unable to sign the client certificate");
  }

  OpenSslPointer<BIO, BIO_free> key_bio(BIO_new(BIO_s_mem()));
  OpenSslPointer<BIO, BIO_free> certificate_bio(BIO_new(BIO_s_mem()));
  if (!key_bio || !certificate_bio ||
      PEM_write_bio_PrivateKey(key_bio.get(), private_key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1 ||
      PEM_write_bio_X509(certificate_bio.get(), certificate.get()) != 1) {
    throw std::runtime_error("Unable to serialize the client credentials");
  }

  private_key_pem_ = bio_bytes(key_bio.get());
  certificate_pem_ = bio_bytes(certificate_bio.get());
}

void ClientIdentity::loadOrCreate() {
  QSettings settings;
  unique_id_ = settings.value(unique_id_key).toString();
  certificate_pem_ = settings.value(certificate_key).toByteArray();
  private_key_pem_ = settings.value(private_key_key).toByteArray();

  const auto certificate_valid = !QSslCertificate(certificate_pem_).isNull();
  const auto key_valid = !QSslKey(private_key_pem_, QSsl::Rsa).isNull();
  if (!certificate_valid || !key_valid) {
    createCredentials();
    settings.setValue(certificate_key, certificate_pem_);
    settings.setValue(private_key_key, private_key_pem_);
  }

  if (unique_id_.isEmpty()) {
    QByteArray random_id(8, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(random_id.data()), static_cast<int>(random_id.size())) != 1) {
      throw std::runtime_error("Unable to generate the client unique ID");
    }
    unique_id_ = QString::fromLatin1(random_id.toHex());
    settings.setValue(unique_id_key, unique_id_);
  }
}
