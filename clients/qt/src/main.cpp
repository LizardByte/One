#include <cstdlib>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "application_list_model.hpp"
#include "host_list_model.hpp"
#include "ui_environment.hpp"

int main(int argc, char* argv[]) {
  QGuiApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("One"));
  QCoreApplication::setApplicationVersion(QStringLiteral(ONE_VERSION));
  QCoreApplication::setOrganizationName(QStringLiteral("LizardByte"));

  UiEnvironment ui_environment;
  HostListModel computer_model;
  ApplicationListModel application_model;
  QObject::connect(
      &computer_model,
      &HostListModel::hostOpened,
      &application_model,
      &ApplicationListModel::loadHost);
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("UiEnvironment"), &ui_environment);
  engine.rootContext()->setContextProperty(QStringLiteral("ComputerModel"), &computer_model);
  engine.rootContext()->setContextProperty(QStringLiteral("ApplicationModel"), &application_model);

  QObject::connect(
      &engine,
      &QQmlApplicationEngine::objectCreationFailed,
      &application,
      []() { QCoreApplication::exit(EXIT_FAILURE); },
      Qt::QueuedConnection);

  engine.loadFromModule(QStringLiteral("One"), QStringLiteral("Main"));

  if (QCoreApplication::arguments().contains(QStringLiteral("--validate-ui"))) {
    return engine.rootObjects().isEmpty() ? EXIT_FAILURE : EXIT_SUCCESS;
  }

  return application.exec();
}
