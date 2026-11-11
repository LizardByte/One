#pragma once

#include <QSet>
#include <QString>
#include <QObject>

namespace QMdnsEngine {
class Browser;
class Server;
class Service;
}  // namespace QMdnsEngine

class HostDiscoveryService final : public QObject {
  Q_OBJECT

 public:
  explicit HostDiscoveryService(QObject* parent = nullptr);

  void start();

 signals:
  void hostDiscovered(const QString& name, const QString& address, quint16 port);

 private:
  void resolve(const QMdnsEngine::Service& service);

  QMdnsEngine::Server* server_ {nullptr};
  QMdnsEngine::Browser* browser_ {nullptr};
  QSet<QString> pending_;
};
