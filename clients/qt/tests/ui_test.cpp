#include <QDir>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

bool expect(const bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

QQuickItem* find_item(QQuickItem* root, const QString& name) {
  if (!root->isVisible()) return nullptr;
  if (root->objectName() == name) return root;
  for (auto* child : root->childItems()) {
    if (auto* item = find_item(child, name)) return item;
  }
  return nullptr;
}

bool click(QQuickWindow* window, const char* name) {
  auto* item = find_item(window->contentItem(), QString::fromLatin1(name));
  if (!expect(item && item->isVisible() && item->isEnabled(), name))
    return false;
  const auto position =
      item->mapToScene(QPointF(item->width() / 2, item->height() / 2))
          .toPoint();
  if (!expect(window->contentItem()->contains(position),
              "Control must be inside the window"))
    return false;
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, position);
  QTest::qWait(300);
  return true;
}

bool capture(QQuickWindow* window, const QString& directory,
             const QString& name) {
  if (directory.isEmpty()) return true;
  QTest::qWait(100);
  return expect(window->grabWindow().save(directory + '/' + name +
                                          QStringLiteral(".png")),
                "Screenshot failed");
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
  // Static Qt's offscreen plugin uses a generic font database. Point it at
  // Windows' own fonts so captures cannot silently pass with missing glyphs.
  if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
    qputenv("QT_QPA_FONTDIR", qgetenv("SystemRoot") + "/Fonts");
#endif
  QGuiApplication app(argc, argv);
#ifdef Q_OS_WIN
  if (!expect(QFontDatabase::families().contains(QStringLiteral("Segoe UI")),
              "UI review must use real Windows fonts, not missing-glyph boxes"))
    return EXIT_FAILURE;
#endif
  QCoreApplication::setApplicationName(QStringLiteral("One UI test"));
  QCoreApplication::setOrganizationName(QStringLiteral("One tests"));
  QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QTemporaryDir settings;
  if (!settings.isValid()) return EXIT_FAILURE;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                     settings.path());

  QString screenshots;
  for (const auto& argument : app.arguments()) {
    if (argument.startsWith(QStringLiteral("--screenshots=")))
      screenshots = argument.sliced(14);
  }
  if (!screenshots.isEmpty() && !QDir().mkpath(screenshots))
    return EXIT_FAILURE;

  bool check_saved_appearance = false;
  for (const auto& mode : {QStringLiteral("desktop"), QStringLiteral("touch"),
                           QStringLiteral("television")}) {
    QQmlApplicationEngine engine;
    bool warnings = false;
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [&warnings](const QList<QQmlError>& errors) {
                       warnings = true;
                       for (const auto& error : errors)
                         std::cerr << error.toString().toStdString() << '\n';
                     });
    QQmlComponent fixture_component(
        &engine, QUrl(QStringLiteral("qrc:/tests/ui_fixture.qml")));
    std::unique_ptr<QObject> fixture(fixture_component.create());
    if (!expect(fixture != nullptr, "UI fixture must load"))
      return EXIT_FAILURE;
    auto* environment = fixture->property("environment").value<QObject*>();
    auto* computers = fixture->property("computers").value<QObject*>();
    auto* applications = fixture->property("applications").value<QObject*>();
    auto* streaming = fixture->property("streaming").value<QObject*>();
    environment->setProperty("mode", mode);
    engine.rootContext()->setContextProperty(QStringLiteral("UiEnvironment"),
                                             environment);
    engine.rootContext()->setContextProperty(QStringLiteral("ComputerModel"),
                                             computers);
    engine.rootContext()->setContextProperty(QStringLiteral("ApplicationModel"),
                                             applications);
    engine.rootContext()->setContextProperty(
        QStringLiteral("StreamingController"), streaming);
    engine.loadFromModule(QStringLiteral("One"), QStringLiteral("Main"));
    if (!expect(!engine.rootObjects().isEmpty(), "Main window must load"))
      return EXIT_FAILURE;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!expect(window != nullptr, "Root must be a window"))
      return EXIT_FAILURE;
    auto* theme = engine.singletonInstance<QObject*>("One", "Theme");
    if (check_saved_appearance &&
        !expect(theme->property("appearance").toInt() == 1,
                "Appearance must persist across UI restarts"))
      return EXIT_FAILURE;
    theme->setProperty("appearance", 0);
    QTest::qWait(250);
    if (!capture(window, screenshots, mode + "-computers")) return EXIT_FAILURE;

    auto* offline =
        find_item(window->contentItem(), QStringLiteral("hostAction2"));
    if (mode != "touch" && !expect(offline && !offline->isEnabled(),
                                   "Offline hosts must not be actionable"))
      return EXIT_FAILURE;
    if (!click(window, "addComputerButton")) return EXIT_FAILURE;
    auto* address =
        find_item(window->contentItem(), QStringLiteral("hostAddress"));
    if (!expect(address && address->hasActiveFocus(),
                "Add dialog must focus its address field"))
      return EXIT_FAILURE;
    for (const char character : QByteArrayLiteral("192.168.1.200"))
      QTest::keyClick(window, character);
    if (!capture(window, screenshots, mode + "-add")) return EXIT_FAILURE;
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(200);
    if (!expect(computers->property("addedAddress").toString() ==
                    QStringLiteral("192.168.1.200"),
                "Enter must add the host"))
      return EXIT_FAILURE;

    // Verify PIN display and result handling on a visible unpaired card.
    if (mode != "touch") {
      if (!click(window, "hostAction1")) return EXIT_FAILURE;
      if (!expect(computers->property("pairedRow").toInt() == 1,
                  "Pair must target the selected host"))
        return EXIT_FAILURE;
      auto* dialog =
          window->findChild<QObject*>(QStringLiteral("pairingDialog"));
      if (!expect(
              dialog && dialog->property("pending").toBool() &&
                  dialog->property("pin").toString() == QStringLiteral("1234"),
              "Pairing PIN must be visible"))
        return EXIT_FAILURE;
      if (!expect(dialog->property("instanceEndpoint").toString() ==
                      "192.168.1.105:48089",
                  "Pairing dialog must identify the selected instance's port"))
        return EXIT_FAILURE;
      if (!capture(window, screenshots, mode + "-pairing")) return EXIT_FAILURE;
      QMetaObject::invokeMethod(computers, "pairingCompleted",
                                Q_ARG(QString, "studio"), Q_ARG(bool, true),
                                Q_ARG(QString, ""));
      if (!expect(!dialog->property("pending").toBool(),
                  "Pair completion must replace the PIN"))
        return EXIT_FAILURE;
      QMetaObject::invokeMethod(dialog, "close");
      QTest::qWait(200);
    }

    if (!click(window, "hostAction0")) return EXIT_FAILURE;
    if (!expect(computers->property("openedRow").toInt() == 0,
                "View games must open the selected host"))
      return EXIT_FAILURE;
    auto* library_stack =
        window->findChild<QObject*>(QStringLiteral("computersStack"));
    auto* library_page =
        library_stack->property("currentItem").value<QObject*>();
    if (!expect(library_page->property("hostEndpoint").toString() ==
                    "192.168.1.105:47989",
                "Library must display its own instance endpoint"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-library")) return EXIT_FAILURE;
    if (!click(window, "refreshApplications")) return EXIT_FAILURE;
    if (!expect(applications->property("refreshCount").toInt() == 1,
                "Refresh must call the application model"))
      return EXIT_FAILURE;
    applications->setProperty("loading", true);
    QTest::qWait(50);
    if (!expect(!find_item(window->contentItem(), "refreshApplications")
                     ->isEnabled(),
                "Loading must disable refresh"))
      return EXIT_FAILURE;
    applications->setProperty("loading", false);
    applications->setProperty(
        "error",
        "This computer is no longer reachable. Try again when it is online.");
    QTest::qWait(50);
    if (!capture(window, screenshots, mode + "-library-error"))
      return EXIT_FAILURE;
    applications->setProperty("pairingRequired", true);
    QTest::qWait(50);
    if (!expect(!find_item(window->contentItem(), "refreshApplications")
                     ->isEnabled(),
                "Unpaired library must disable refresh"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-library-unpaired"))
      return EXIT_FAILURE;
    if (!click(window, "libraryRecovery") || !click(window, "hostAction0"))
      return EXIT_FAILURE;
    applications->setProperty("pairingRequired", false);
    applications->setProperty("error", "");
    QTest::qWait(50);
    if (!click(window, "gameCard0")) return EXIT_FAILURE;
    if (!expect(streaming->property("startedId").toInt() == 1,
                "Game card must launch its own application ID"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-connecting"))
      return EXIT_FAILURE;
    // Back through the real page signal also exercises streaming teardown.
    auto* stack = window->findChild<QObject*>(QStringLiteral("computersStack"));
    if (!expect(stack != nullptr, "Computers stack must exist"))
      return EXIT_FAILURE;
    auto* stream_page = stack->property("currentItem").value<QObject*>();
    QMetaObject::invokeMethod(stream_page, "leaveStream");
    QTest::qWait(300);
    if (!expect(streaming->property("stopCount").toInt() > 0,
                "Leaving the stream must stop it"))
      return EXIT_FAILURE;

    if (!click(window, "navigation1")) return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-settings")) return EXIT_FAILURE;
    QTest::keyClick(window, mode == "desktop" ? Qt::Key_Right : Qt::Key_Down);
    QTest::qWait(50);
    if (!expect(
            find_item(window->contentItem(), "appearance0")->hasActiveFocus(),
            "Navigation must transfer keyboard focus to page controls"))
      return EXIT_FAILURE;
    QTest::keyClick(window, Qt::Key_Right);
    if (!expect(
            find_item(window->contentItem(), "appearance1")->hasActiveFocus(),
            "Arrow keys must move between appearance options"))
      return EXIT_FAILURE;
    QTest::keyClick(window, Qt::Key_Space);
    QTest::qWait(100);
    if (!expect(window->color() == QColor("#F3F6F5"),
                "Light appearance must change the window palette"))
      return EXIT_FAILURE;
    if (!expect(
            theme->property("accentText").value<QColor>() == QColor("#FFFFFF"),
            "Light primary buttons need white text"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-light-settings"))
      return EXIT_FAILURE;
    if (!click(window, "navigation0")) return EXIT_FAILURE;
    if (mode != "television") {
      const auto size = window->size();
      window->resize(mode == "touch" ? QSize(360, 600) : QSize(800, 600));
      QTest::qWait(150);
      if (!capture(window, screenshots, mode + "-small-library"))
        return EXIT_FAILURE;
      window->resize(size);
      QTest::qWait(150);
    }
    if (!capture(window, screenshots, mode + "-light-library"))
      return EXIT_FAILURE;
    if (!click(window, "backToComputers")) return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-light-computers"))
      return EXIT_FAILURE;
    if (!click(window, "navigation1") || !click(window, "appearance0") ||
        !click(window, "navigation2"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-about")) return EXIT_FAILURE;

    if (!click(window, "navigation0")) return EXIT_FAILURE;
    if (mode != "television") {
      const auto size = window->size();
      window->resize(mode == "touch" ? QSize(360, 600) : QSize(800, 600));
      QTest::qWait(150);
      if (!capture(window, screenshots, mode + "-small-computers"))
        return EXIT_FAILURE;
      auto* action =
          find_item(window->contentItem(), QStringLiteral("hostAction0"));
      if (!expect(action != nullptr,
                  "Small windows must retain their host action"))
        return EXIT_FAILURE;
      for (auto* ancestor = action->parentItem(); ancestor;
           ancestor = ancestor->parentItem()) {
        if (ancestor->clip() &&
            !expect(ancestor->boundingRect().contains(action->mapRectToItem(
                        ancestor, action->boundingRect())),
                    "Small window must not clip the host action"))
          return EXIT_FAILURE;
      }
      window->resize(size);
      QTest::qWait(150);
    }
    QMetaObject::invokeMethod(computers, "clear");
    QTest::qWait(150);
    if (!capture(window, screenshots, mode + "-empty")) return EXIT_FAILURE;
    if (mode == "touch") {
      window->resize(360, 600);
      QTest::qWait(150);
      if (!capture(window, screenshots, mode + "-small-empty"))
        return EXIT_FAILURE;
    }
    if (!expect(!warnings,
                "UI must not emit QML binding, layout, or runtime warnings"))
      return EXIT_FAILURE;
    // Destroy the window before its injected model objects.
    theme->setProperty("appearance", 1);
    check_saved_appearance = true;
    delete window;
    std::cout << mode.toStdString() << " UI interactions passed\n";
  }
  return EXIT_SUCCESS;
}
