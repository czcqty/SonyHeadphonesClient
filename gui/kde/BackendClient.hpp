#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QPointer>
#include <functional>
#include <memory>
#include <QJsonArray>

// Presentation transport only. All device validation remains in the backend.
class BackendClient : public QObject {
public:
    QJsonObject state;
    QString error, diagnostic;
    bool ready = false;
    int sent = 0;
    std::function<void()> changed;
    int startupTimeout = 10000;

    explicit BackendClient(QObject* parent = nullptr) : QObject(parent) {}
    ~BackendClient() override { shutdown(); }
    void start(const QString& path, const QStringList& arguments) {
        executable = path; args = arguments; restart();
    }
    void restart() {
        retire();
        ready = false; state = {}; diagnostic.clear(); error = "backend-starting";
        notify();
        const auto revision = ++generation;
        if (retiring && retiring->state() != QProcess::NotRunning) {
            connect(retiring, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, revision](int, QProcess::ExitStatus) { if (revision == generation) launch(); });
        } else launch();
    }
private:
    void launch() {
        auto source = process = new QProcess(this);
        auto pending = std::make_shared<QByteArray>();
        auto timer = new QTimer(source);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, source] {
            if (process == source && !ready) fail("backend-start-timeout");
        });
        connect(source, &QProcess::readyReadStandardError, this, [this, source] {
            if (process == source) diagnostic = (diagnostic + QString::fromUtf8(source->readAllStandardError())).right(4096);
        });
        connect(source, &QProcess::readyReadStandardOutput, this, [this, source, pending, timer] {
            if (process != source) return;
            *pending += source->readAllStandardOutput();
            while (pending->contains('\n')) {
                const auto end = pending->indexOf('\n');
                if (end > 1048576) { fail("backend-protocol-error"); return; }
                QJsonParseError parse;
                const auto document = QJsonDocument::fromJson(pending->left(end), &parse);
                pending->remove(0, end + 1);
                if (parse.error != QJsonParseError::NoError || !document.isObject()) { fail("backend-protocol-error"); return; }
                const auto event = document.object();
                if (event.value("type") == "state") {
                    if (event.value("version").toInt() != 1) { fail("unsupported-version"); return; }
                    for (auto key : {"app", "connection", "playback", "noiseControl", "equalizer", "settings", "deviceManager"})
                        if (!event.value(key).isObject()) { fail("backend-protocol-error"); return; }
                    if (!ready) error.clear();
                    state = event; ready = true; timer->stop();
                } else if (event.value("type") == "result") {
                    error = event.value("ok").toBool() ? QString() : event.value("errorCode").toString();
                }
                notify();
            }
            if (pending->size() > 1048576) fail("backend-protocol-error");
        });
        connect(source, &QProcess::errorOccurred, this, [this, source](QProcess::ProcessError code) {
            if (process == source) { diagnostic = source->errorString(); fail(code == QProcess::FailedToStart ? "backend-start-failed" : "backend-stopped"); }
        });
        connect(source, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, source](int, QProcess::ExitStatus) {
            if (process == source) {
                diagnostic = (diagnostic + QString::fromUtf8(source->readAllStandardError())).right(4096);
                fail("backend-stopped");
            }
        });
        timer->start(startupTimeout);
        source->start(executable, args);
    }
public:
    void send(const QString& target, const QString& method, const QJsonArray& args) {
        if (!ready || !process) return;
        if (process->bytesToWrite() > 65536) { fail("backend-write-failed"); return; }
        const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"id", QString::number(sent + 1)},
            {"target", target}, {"method", method}, {"args", args}}).toJson(QJsonDocument::Compact) + '\n';
        if (process->write(data) < 0) fail("backend-write-failed"); else ++sent;
    }
    void shutdown() {
        ++generation; ready = false;
        if (!process) return;
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning) {
            process->closeWriteChannel();
            if (!process->waitForFinished(2000)) { process->kill(); process->waitForFinished(1000); }
        }
        delete process; process = nullptr;
    }
private:
    QProcess* process = nullptr;
    QPointer<QProcess> retiring;
    quint64 generation = 0;
    QString executable;
    QStringList args;
    void notify() { if (changed) changed(); }
    void retire() {
        if (!process) return;
        auto old = process; process = nullptr; retiring = old;
        old->disconnect(this);
        if (old->state() == QProcess::NotRunning) old->deleteLater();
        else {
            connect(old, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), old, &QObject::deleteLater);
            old->kill(); // Discard unsent requests; never replay on the new child.
        }
    }
    void fail(const QString& code) { ++generation; ready = false; state = {}; error = code; retire(); notify(); }
};
