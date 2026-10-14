#include "BackendProtocol.hpp"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QJsonDocument>
#include <QLockFile>
#include <QSemaphore>
#include <QStandardPaths>
#include <QTimer>
#include <iostream>
#include <thread>

namespace {
void publish(const QJsonObject& object)
{
    std::cout << QJsonDocument(object).toJson(QJsonDocument::Compact).constData() << std::endl;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("SonyHeadphonesClient");
    app.setOrganizationName("czcqty");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"live", "Connect to real Bluetooth devices (default: isolated preview)"});
    parser.addOption({"profile", "Isolated preview device profile", "profile", "WH-1000XM6"});
    parser.process(app);
    const bool live = parser.isSet("live");
    if (live && parser.isSet("profile")) { std::cerr << "--profile cannot be combined with --live\n"; return 2; }

    // Multiple native presentations must not race for the same Bluetooth device.
    std::unique_ptr<QLockFile> lock;
    if (live) {
        const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (directory.isEmpty() || !QDir().mkpath(directory)) return 3;
        lock = std::make_unique<QLockFile>(directory + "/device-backend.lock");
        lock->setStaleLockTime(0); // Only reclaim a lock when its owner has exited.
        if (!lock->tryLock(0)) { std::cerr << "Another live backend owns the device session\n"; return 3; }
    }
    DeviceService service(!live);
    if (!live && !service.applyProfile(parser.value("profile"))) return 2;
    BackendProtocol protocol(service);
    QTimer pending;
    pending.setSingleShot(true);
    const auto snapshot = [&] { publish(protocol.snapshot()); };
    const auto schedule = [&] { if (!pending.isActive()) pending.start(0); };
    QObject::connect(&pending, &QTimer::timeout, &app, snapshot);
    QObject::connect(service.playback(), &PlaybackViewModel::changed, &app, schedule);
    QObject::connect(service.noiseControl(), &NoiseControlViewModel::changed, &app, schedule);
    QObject::connect(service.equalizer(), &EqualizerViewModel::changed, &app, schedule);
    QObject::connect(service.settings(), &SettingsViewModel::changed, &app, schedule);
    QObject::connect(service.deviceManager(), &DeviceManagerViewModel::changed, &app, schedule);
    QObject::connect(service.connection(), &ConnectionViewModel::changed, &app, schedule);
    QObject::connect(&service, &DeviceService::profileChanged, &app, schedule);

    // Bound both individual frames and queued work. The pipe supplies natural
    // backpressure; input never performs a device operation on the reader thread.
    QSemaphore capacity(64);
    std::thread input([&] {
        std::string line;
        bool oversized = false;
        const auto submit = [&] {
            if (line.empty() && !oversized) return;
            capacity.acquire();
            const auto bytes = QByteArray::fromStdString(line);
            const bool tooLarge = oversized;
            QMetaObject::invokeMethod(&app, [&, bytes, tooLarge] {
                const auto document = QJsonDocument::fromJson(bytes);
                pending.stop();
                if (tooLarge || !document.isObject())
                    publish({{"type", "result"}, {"version", BackendProtocol::Version}, {"ok", false},
                        {"status", "rejected"}, {"errorCode", tooLarge ? "frame-too-large" : "invalid-json"},
                        {"error", tooLarge ? "Frame exceeds 65536 bytes" : "Expected a JSON object"}});
                else publish(protocol.execute(document.object()));
                // A result is always followed by a full snapshot before the next
                // command is handled; asynchronous device changes also publish.
                pending.stop();
                snapshot();
                capacity.release();
            }, Qt::QueuedConnection);
            line.clear(); oversized = false;
        };
        char ch;
        while (std::cin.get(ch)) {
            if (ch == '\n') submit();
            else if (line.size() < 65536) line.push_back(ch);
            else oversized = true;
        }
        submit(); // Handle a final frame without a newline, then drain and exit.
        QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    snapshot();
    const int result = app.exec();
    input.join();
    return result;
}
