// shark — configure the Attack Shark R1 gaming mouse from Omarchy.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>

#include <cstdio>

#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    if (arguments.contains("--version")) {
        std::printf("shark 0.1.0\n");
        return 0;
    }

    // `shark status`, `apply`, `battery`, `set` and friends talk to USB and
    // print JSON, so they must run without a display.
    const QString command = arguments.value(1);
    const bool shot = command == "shot";
    const bool headless = (!command.isEmpty() && command != "open" && !command.startsWith("-")) || shot;
    if (headless)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("shark");
    app.setApplicationVersion("0.1.0");

    // Matches the window to shark.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("shark");
    app.setWindowIcon(QIcon::fromTheme("shark"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    if (headless) {
        if (shot) {
            // Renders the window offscreen at its design size, so the README
            // screenshot never depends on the desktop layout.
            QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("translator", &translator);
            engine.rootContext()->setContextProperty("theme", &theme);
            engine.rootContext()->setContextProperty("backend", &backend);
            engine.load(QUrl("qrc:/Main.qml"));
            if (engine.rootObjects().isEmpty())
                return 1;
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            const QString target = arguments.value(2);
            const int shotWidth = arguments.value(3).toInt() > 0 ? arguments.value(3).toInt() : 900;
            const int shotHeight = arguments.value(4).toInt() > 0 ? arguments.value(4).toInt() : 640;
            QTimer::singleShot(700, &app, [window, target, shotWidth, shotHeight, &app] {
                window->resize(shotWidth, shotHeight);
                QTimer::singleShot(250, &app, [window, target] {
                    const QImage image = window->grabWindow();
                    const QString path = target.isEmpty() ? QStringLiteral("shark.png") : target;
                    image.save(path) ? std::printf("wrote %s\n", qPrintable(path)) : std::printf("failed\n");
                    QCoreApplication::exit(image.isNull() ? 1 : 0);
                });
            });
            return app.exec();
        }
        QObject::connect(&backend, &Backend::cliFinished, &app, &QGuiApplication::exit);
        // Queued, so the command runs inside the event loop.
        QTimer::singleShot(0, &backend, [&backend, arguments] { backend.runCli(arguments); });
        return app.exec();
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
