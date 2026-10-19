#include "BackendClient.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() != 3) return 2;
    BackendClient client;
    auto start = [&](const QString& mode) { client.start(arguments[1], {arguments[2], "--fixture", mode}); };
    auto wait = [&](const std::function<bool()>& predicate) {
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 6000) {
            app.processEvents();
            if (predicate()) return true;
            QThread::msleep(5);
        }
        std::cerr << "Timeout: " << client.error.toStdString() << '\n';
        return false;
    };
    client.start("/soundconnect-missing/backend", {});
    if (!wait([&] { return client.error == "backend-start-failed"; })) return 1;
    client.startupTimeout = 150;
    start("timeout");
    if (!wait([&] { return client.error == "backend-start-timeout"; })) return 1;
    client.startupTimeout = 10000;
    for (auto mode : {"version", "invalid", "busy"}) {
        start(mode);
        if (!wait([&] { return client.error != "backend-starting"; }) || client.ready || !client.state.isEmpty()) return 1;
    }
    start("noisy");
    if (!wait([&] { return client.ready && client.diagnostic.endsWith("TAIL"); }) || client.diagnostic.size() > 4096) return 1;
    for (int i = 0; i < 20; ++i) client.send("app", "test", {i});
    if (!wait([&] { return client.state.value("app").toObject().value("lastId") == "20"; })) return 1;
    client.restart();
    if (!wait([&] { return client.ready; }) || client.state.value("app").toObject().value("writes").toInt() != 0 || client.sent != 20) return 1;
    start("stop");
    if (!wait([&] { return client.ready; }) || !wait([&] { return client.error == "backend-stopped"; }) || !client.state.isEmpty()) return 1;
    start("hang");
    if (!wait([&] { return client.ready; })) return 1;
    client.shutdown();
    std::cout << "PASS: KDE transport failures, ordered writes, recovery and shutdown\n";
    return 0;
}
