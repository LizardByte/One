#include "host_discovery_service.hpp"

#include <memory>

#include <QHostAddress>
#include <QList>
#include <QAbstractSocket>
#include <QTimer>

#include <qmdnsengine/browser.h>
#include <qmdnsengine/resolver.h>
#include <qmdnsengine/server.h>
#include <qmdnsengine/service.h>

HostDiscoveryService::HostDiscoveryService(QObject* parent) : QObject(parent) {}

void HostDiscoveryService::start() {
  if (browser_ != nullptr) {
    return;
  }

  server_ = new QMdnsEngine::Server(this);
  browser_ = new QMdnsEngine::Browser(server_, QByteArrayLiteral("_nvstream._tcp.local."), nullptr, this);
  connect(browser_, &QMdnsEngine::Browser::serviceAdded, this, &HostDiscoveryService::resolve);
  connect(browser_, &QMdnsEngine::Browser::serviceUpdated, this, &HostDiscoveryService::resolve);
}

void HostDiscoveryService::resolve(const QMdnsEngine::Service& service) {
  const auto hostname = service.hostname();
  const auto port = service.port();
  const auto key = QString::fromUtf8(hostname) + u':' + QString::number(port);
  if (hostname.isEmpty() || port == 0 || pending_.contains(key)) {
    return;
  }
  pending_.insert(key);

  const auto display_name = QString::fromUtf8(service.name());
  auto addresses = std::make_shared<QList<QHostAddress>>();
  auto* resolver = new QMdnsEngine::Resolver(server_, hostname, nullptr, this);
  auto* timeout = new QTimer(resolver);
  timeout->setSingleShot(true);

  auto finish = [this, resolver, addresses, display_name, port, key](const QHostAddress& preferred) {
    if (!pending_.remove(key)) {
      return;
    }
    if (!preferred.isNull()) {
      emit hostDiscovered(display_name, preferred.toString(), port);
    }
    resolver->deleteLater();
  };

  connect(resolver, &QMdnsEngine::Resolver::resolved, resolver, [addresses, finish](const QHostAddress& address) {
    if (address.isNull() || address.isLoopback()) {
      return;
    }
    addresses->append(address);
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
      finish(address);
    }
  });

  connect(timeout, &QTimer::timeout, resolver, [addresses, finish]() {
    QHostAddress best;
    for (const auto& address : *addresses) {
      if (!address.isNull()) {
        best = address;
        break;
      }
    }
    finish(best);
  });
  timeout->start(3000);
}
