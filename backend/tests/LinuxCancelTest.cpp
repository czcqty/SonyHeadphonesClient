#include "MdrSession.hpp"

#include <QCoreApplication>
#include <QSemaphore>

#include <atomic>
#include <cstdio>
#include <mdr-c/Platform/PlatformLinux.h>

namespace {
// Replace only the platform factory in this test executable. Model an SDP
// handle acquired just before the nonblocking connect worker returns.
QSemaphore connecting, disconnected;
std::atomic_bool handleOpen{false};

int connectDelayed(void*, const char*, const char*)
{
    connecting.release();
    // An early disconnect wakes us before the handle has been published.
    // With correct teardown the worker finishes first.
    disconnected.tryAcquire(1, 200);
    handleOpen = true;
    return MDR_RESULT_INPROGRESS;
}

void disconnectHandle(void*)
{
    handleOpen = false;
    disconnected.release();
}
}

struct MDRConnectionLinux {
    MDRConnection transport{.connect = connectDelayed, .disconnect = disconnectHandle};
};

extern "C" MDRConnectionLinux* mdrConnectionLinuxCreate() { return new MDRConnectionLinux; }
extern "C" MDRConnection* mdrConnectionLinuxGet(MDRConnectionLinux* p) { return &p->transport; }
extern "C" void mdrConnectionLinuxDestroy(MDRConnectionLinux* p) { delete p; }

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    DeviceState device;
    MdrSession session(&device);
    session.connectDevice("test", "12:34:56:78:9A:BC");
    if (!connecting.tryAcquire(1, 2000)) {
        std::fputs("Connection worker did not start\n", stderr);
        return 1;
    }
    session.disconnectDevice();
    if (handleOpen) {
        std::fputs("Cancelled Linux connection retained a late SDP handle\n", stderr);
        return 1;
    }
    return 0;
}
