#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdlib>

#include "application_list_model.hpp"
#include "host_list_model.hpp"
#include "streaming_controller.hpp"
#include "ui_environment.hpp"

int main(int argc, char* argv[]) {
  QGuiApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("One"));
  QCoreApplication::setApplicationVersion(QStringLiteral(ONE_VERSION));
  QCoreApplication::setOrganizationName(QStringLiteral("LizardByte"));
  QQuickStyle::setStyle(QStringLiteral("Basic"));

  const bool validating_ui = QCoreApplication::arguments().contains(QStringLiteral("--validate-ui"));
  QTemporaryDir validation_settings;
  if (validating_ui) {
    if (!validation_settings.isValid()) return EXIT_FAILURE;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, validation_settings.path());
  }

  UiEnvironment ui_environment;
  HostListModel computer_model(nullptr, !validating_ui);
  ApplicationListModel application_model;
  StreamingController streaming_controller;
  QObject::connect(&application_model, &ApplicationListModel::hostAuthenticationFailed, &computer_model,
                   &HostListModel::requirePairing);
  QObject::connect(&streaming_controller, &StreamingController::hostAuthenticationFailed, &computer_model,
                   &HostListModel::requirePairing);
  QObject::connect(&streaming_controller, &StreamingController::hostAuthenticationFailed, &application_model,
                   &ApplicationListModel::requirePairing);
  QObject::connect(&computer_model, &HostListModel::hostOpened, &application_model, &ApplicationListModel::loadHost);
  QObject::connect(&computer_model, &HostListModel::hostOpened, &streaming_controller,
                   &StreamingController::selectHost);
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("UiEnvironment"), &ui_environment);
  engine.rootContext()->setContextProperty(QStringLiteral("ComputerModel"), &computer_model);
  engine.rootContext()->setContextProperty(QStringLiteral("ApplicationModel"), &application_model);
  engine.rootContext()->setContextProperty(QStringLiteral("StreamingController"), &streaming_controller);

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      []() { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

  engine.loadFromModule(QStringLiteral("One"), QStringLiteral("Main"));

  if (validating_ui) {
    return engine.rootObjects().isEmpty() ? EXIT_FAILURE : EXIT_SUCCESS;
  }

  return application.exec();
}
