#include "DBusHelper.hpp"

#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>

#include "../Platform.hpp"
#include <mdr-c/Platform/PlatformLinux.h>

struct MDRConnectionLinux
{
    MDRConnection mdrConn;

    std::string lastError;
    int fd;

    MDRConnectionLinux() noexcept :
        mdrConn({
            .user = this,
            .connect = Connect,
            .disconnect = Disconnect,
            .recv = Recv,
            .send = Send,
            .poll = Poll,
            .getDevicesList = GetDevicesList,
            .freeDevicesList = FreeDevicesList,
            .getLastError = GetLastError
        }),
        fd(-1)
    {
    }

    ~MDRConnectionLinux() { Disconnect(this); }

    sdp_session* sdpSession{nullptr};
    uint8_t uuid[16];
    std::string macAddress;

    static int Connect(void* user, const char* macAddress, const char* serviceUUID) noexcept
    {
        auto* ptr = static_cast<MDRConnectionLinux*>(user);
        Disconnect(ptr);
        if (serviceUUIDtoBytes(serviceUUID, ptr->uuid) != 0)
            return MDR_RESULT_ERROR_BAD_ADDRESS;
        ptr->fd = socket(AF_BLUETOOTH, SOCK_STREAM | SOCK_NONBLOCK, BTPROTO_RFCOMM);
        if (ptr->fd < 0)
        {
            ptr->lastError = "Failed to create RFCOMM socket";
            return MDR_RESULT_ERROR_NET;
        }
        unsigned int linkmode = RFCOMM_LM_AUTH | RFCOMM_LM_ENCRYPT;
        setsockopt(ptr->fd, SOL_RFCOMM, RFCOMM_LM, &linkmode, sizeof(linkmode));
        ptr->macAddress = macAddress;
        ptr->sdpSession = sdp_connect_nb(macAddress);
        if (!ptr->sdpSession)
        {
            Disconnect(ptr);
            ptr->lastError = "Failed to connect to remote SDP server";
            return MDR_RESULT_ERROR_NET;
        }
        ptr->lastError = "Connecting to remote SDP server";
        return MDR_RESULT_INPROGRESS;
    }

    static void Disconnect(void* user) noexcept
    {
        auto* ptr = static_cast<MDRConnectionLinux*>(user);
        if (ptr->fd >= 0)
            close(ptr->fd), ptr->fd = -1;
        if (ptr->sdpSession)
            sdp_close(ptr->sdpSession), ptr->sdpSession = nullptr;
    }

    static int Recv(void* user, char* dst, int size, int* pReceived) noexcept
    {
        auto* ptr = static_cast<MDRConnectionLinux*>(user);
        if (ptr->fd < 0)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        ssize_t received = recv(ptr->fd, dst, size, 0);
        if (received == 0)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        if (received < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return MDR_RESULT_INPROGRESS;
            ptr->lastError = strerror(errno);
            return MDR_RESULT_ERROR_NET;
        }
        *pReceived = static_cast<int>(received);
        return MDR_RESULT_OK;
    }

    static int Send(void* user, const char* src, int size, int* pSent) noexcept
    {
        auto* ptr = static_cast<MDRConnectionLinux*>(user);
        if (ptr->fd < 0)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        ssize_t sent = send(ptr->fd, src, size, 0);
        if (sent == 0)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return MDR_RESULT_INPROGRESS;
            ptr->lastError = strerror(errno);
            return MDR_RESULT_ERROR_NET;
        }
        *pSent = static_cast<int>(sent);
        return MDR_RESULT_OK;
    }

    static int Poll(void* user, int timeout) noexcept
    {
        auto* ptr = static_cast<MDRConnectionLinux*>(user);
        // SDP query not done yet - poll for that
        // before connecting our own socket
        if (ptr->sdpSession)
        {
            int res = sdp_poll(ptr->sdpSession, timeout);
            if (res == 0)
            {
                ptr->lastError = "Waiting for SDP connection to be ready";
                return MDR_RESULT_INPROGRESS;
            }
            if (res < 0)
            {
                ptr->lastError = "Failed to open SDP connection to remote device";
                return MDR_RESULT_ERROR_NET;
            }
            uint8_t ch = sdp_getServiceChannel(ptr->sdpSession, ptr->uuid);
            if (ch == 0)
            {
                ptr->lastError = "Failed to get RFCOMM service channel";
                return MDR_RESULT_ERROR_NET;
            }
            sdp_close(ptr->sdpSession);
            ptr->sdpSession = nullptr;
            // Try to connect now
            sockaddr_rc addr{
                .rc_family = AF_BLUETOOTH,
                .rc_channel = ch
            };
            str2ba(ptr->macAddress.c_str(), &addr.rc_bdaddr);
            res = connect(ptr->fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            if (res != 0 && errno != EINPROGRESS)
            {
                close(ptr->fd);
                ptr->fd = -1;
                ptr->lastError = strerror(errno);
                return MDR_RESULT_ERROR_NET;
            }
            ptr->lastError = "Connecting to remote device";
            return MDR_RESULT_INPROGRESS;
        }
        if (ptr->fd < 0)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        pollfd pfd{
            .fd = ptr->fd,
            .events = POLLIN | POLLOUT | POLLHUP | POLLERR,
        };
        int res = poll(&pfd, 1, timeout);
        if (pfd.revents & POLLHUP || pfd.revents & POLLERR)
            res = -1;
        if (res > 0)
            return MDR_RESULT_OK;
        if (res == 0)
            return MDR_RESULT_ERROR_TIMEOUT;
        ptr->lastError = strerror(errno);
        return MDR_RESULT_ERROR_NET;
    }

    static int GetDevicesList(void* user, MDRDeviceInfo** ppList, int* pCount) noexcept
    {
        auto* ptr = static_cast<MDRConnectionLinux*>(user);
        *ppList = nullptr;
        *pCount = 0;
        std::vector<MDRDeviceInfo> devices;
        const int result = bluez_list_devices(devices, ptr->lastError);
        if (result != MDR_RESULT_OK || devices.empty())
            return result;
        *ppList = new MDRDeviceInfo[devices.size()];
        std::copy(devices.begin(), devices.end(), *ppList);
        *pCount = static_cast<int>(devices.size());
        return MDR_RESULT_OK;
    }

    static int FreeDevicesList(void*, MDRDeviceInfo** ppList) noexcept
    {
        if (*ppList)
        {
            delete[] *ppList;
            *ppList = nullptr;
        }
        return MDR_RESULT_OK;
    }

    static const char* GetLastError(void* user) noexcept
    {
        auto* ptr = static_cast<MDRConnectionLinux*>(user);
        return ptr->lastError.c_str();
    }
};

extern "C" {
MDRConnectionLinux* mdrConnectionLinuxCreate() { return new MDRConnectionLinux(); }
void mdrConnectionLinuxDestroy(MDRConnectionLinux* instance) { delete instance; }
MDRConnection* mdrConnectionLinuxGet(MDRConnectionLinux* instance) { return &instance->mdrConn; }
}
