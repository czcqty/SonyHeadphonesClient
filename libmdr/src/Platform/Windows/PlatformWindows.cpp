#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <BluetoothAPIs.h>
#include <rpc.h>
#include <winsock2.h>
#include <ws2bth.h>

#include <fmt/format.h>
#include <string>
#include <vector>

#include "../Platform.hpp"
#include <mdr-c/Platform/PlatformWindows.h>

static bool gWSAStartup = false;
struct MDRConnectionWindows
{
    MDRConnection mdrConn;
    std::string lastError;

    SOCKET conn;
    MDRConnectionWindows() noexcept :
        lastError(""), mdrConn({.user = this,
                                .connect = Connect,
                                .disconnect = Disconnect,
                                .recv = Recv,
                                .send = Send,
                                .poll = Poll,
                                .getDevicesList = GetDevicesList,
                                .freeDevicesList = FreeDevicesList,
                                .getLastError = GetLastError}),
        conn(INVALID_SOCKET)
    {
    }
    static std::string ToU8String(WCHAR* szMessage)
    {
        char szMessageU8[1024];
        WideCharToMultiByte(CP_UTF8, 0, szMessage, -1, szMessageU8, sizeof(szMessageU8), NULL, NULL);
        return szMessageU8;
    }
    static std::string FormatErrorString(DWORD err)
    {
        WCHAR szMessage[1024];
        if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), szMessage,
                           _countof(szMessage), NULL) == 0)
            return "Unknown Error";
        std::string res = ToU8String(szMessage);
        while (!res.empty() && (res.back() == '\n' || res.back() == '\r'))
            res.pop_back();
        return res;
    }
    static int Connect(void* user, const char* macAddress, const char* serviceUUID) noexcept
    {
        auto* ptr = static_cast<MDRConnectionWindows*>(user);
        fprintf(stderr, "[BT-DEBUG] Connect called: mac=%s uuid=%s\n", macAddress, serviceUUID);
        if (!gWSAStartup)
        {
            WSAData data;
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            {
                fprintf(stderr, "[BT-DEBUG] WSAStartup FAILED: %d\n", WSAGetLastError());
                ptr->lastError = FormatErrorString(WSAGetLastError());
                return MDR_RESULT_ERROR_NET;
            }
            gWSAStartup = true;
            fprintf(stderr, "[BT-DEBUG] WSAStartup OK\n");
        }
        ptr->conn = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        fprintf(stderr, "[BT-DEBUG] socket() returned %lld\n", (long long)ptr->conn);
        if (ptr->conn == INVALID_SOCKET)
        {
            const int error = WSAGetLastError();
            fprintf(stderr, "[BT-DEBUG] socket is INVALID_SOCKET: %d\n", error);
            ptr->lastError = FormatErrorString(error);
            return MDR_RESULT_ERROR_NET;
        }

        ULONG nonblock = 1;
        if (ioctlsocket(ptr->conn, FIONBIO, &nonblock) != 0)
        {
            const int error = WSAGetLastError();
            fprintf(stderr, "[BT-DEBUG] ioctlsocket FIONBIO FAILED: %d\n", error);
            ptr->lastError = FormatErrorString(error);
            closesocket(ptr->conn);
            ptr->conn = INVALID_SOCKET;
            return MDR_RESULT_ERROR_NET;
        }
        ULONG enable = TRUE;
        ::setsockopt(ptr->conn, SOL_RFCOMM, SO_BTH_AUTHENTICATE, reinterpret_cast<char*>(&enable), sizeof(enable));
        ::setsockopt(ptr->conn, SOL_RFCOMM, SO_BTH_ENCRYPT, reinterpret_cast<char*>(&enable), sizeof(enable));
        SOCKADDR_BTH sab = {0};
        sab.addressFamily = AF_BTH;
        RPC_STATUS errCode = ::UuidFromStringA((RPC_CSTR)serviceUUID, &sab.serviceClassId);
        if (errCode != RPC_S_OK)
        {
            fprintf(stderr, "[BT-DEBUG] UuidFromStringA FAILED: %ld\n", errCode);
            closesocket(ptr->conn);
            ptr->lastError = FormatErrorString(errCode);
            return MDR_RESULT_ERROR_NET;
        }
        sab.btAddr = macAddressToULL(macAddress);
        fprintf(stderr, "[BT-DEBUG] Connecting to btAddr=0x%llX, serviceClassId={%08lX-%04X-%04X-...}\n",
                (unsigned long long)sab.btAddr, sab.serviceClassId.Data1, sab.serviceClassId.Data2, sab.serviceClassId.Data3);
        int ret = ::connect(ptr->conn, (sockaddr*)&sab, sizeof(sab));
        if (ret == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            fprintf(stderr, "[BT-DEBUG] connect() returned SOCKET_ERROR, WSA err=%d\n", err);
            if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS)
            {
                fprintf(stderr, "[BT-DEBUG] Connect FAILED (not WOULDBLOCK): %s\n", FormatErrorString(err).c_str());
                closesocket(ptr->conn);
                ptr->lastError = FormatErrorString(err);
                return MDR_RESULT_ERROR_NET;
            }
            fprintf(stderr, "[BT-DEBUG] Connect in progress (WOULDBLOCK/INPROGRESS) - normal for non-blocking\n");
        }
        ptr->lastError = "Connecting to WSA Bluetooth socket";
        return MDR_RESULT_INPROGRESS;
    }

    static void Disconnect(void* user) noexcept
    {
        auto* ptr = static_cast<MDRConnectionWindows*>(user);
        if (ptr->conn != INVALID_SOCKET)
        {
            shutdown(ptr->conn, SD_BOTH);
            closesocket(ptr->conn);
            ptr->conn = INVALID_SOCKET;
        }
    }

    static int Recv(void* user, char* dst, int size, int* pReceived) noexcept
    {
        auto* ptr = static_cast<MDRConnectionWindows*>(user);
        if (ptr->conn == INVALID_SOCKET)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        int receieved = recv(ptr->conn, dst, size, 0);
        if (receieved == 0)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        if (receieved < 0)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS)
                return MDR_RESULT_INPROGRESS;
            ptr->lastError = FormatErrorString(err);
            return MDR_RESULT_ERROR_NET;
        }
        *pReceived = receieved;
        return MDR_RESULT_OK;
    }

    static int Send(void* user, const char* src, int size, int* pSent) noexcept
    {
        auto* ptr = static_cast<MDRConnectionWindows*>(user);
        if (ptr->conn == INVALID_SOCKET)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        int sent = send(ptr->conn, src, size, 0);
        if (sent == 0)
            return MDR_RESULT_ERROR_NO_CONNECTION;
        if (sent < 0)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS)
                return MDR_RESULT_INPROGRESS;
            ptr->lastError = FormatErrorString(err);
            return MDR_RESULT_ERROR_NET;
        }
        *pSent = sent;
        return MDR_RESULT_OK;
    }

static int Poll(void* user, int timeout) noexcept
{
    auto* ptr = static_cast<MDRConnectionWindows*>(user);
    if (ptr->conn == INVALID_SOCKET)
        return MDR_RESULT_ERROR_NO_CONNECTION;

    // Set up the file descriptor sets
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    // Add your single socket to all three sets
    FD_SET(ptr->conn, &readfds);
    FD_SET(ptr->conn, &writefds);
    FD_SET(ptr->conn, &exceptfds);

    // `select` uses a timeval struct, not a plain millisecond int
    struct timeval tv;
    tv.tv_sec = timeout / 1000; // Convert ms to seconds
    tv.tv_usec = (timeout % 1000) * 1000; // Convert remainder ms to microseconds

    // Note: The first parameter (nfds) is ignored in Winsock.
    // Pass a NULL timeval for an infinite wait (if timeout < 0)
    int res = select(0, &readfds, &writefds, &exceptfds, (timeout < 0) ? NULL : &tv);

    if (res > 0)
    {
        // A non-blocking connect *fails* by signaling the exception set.
        if (FD_ISSET(ptr->conn, &exceptfds))
        {
            int err = 0;
            int errLen = sizeof(err);
            // You MUST get the actual error code from the socket
            if (getsockopt(ptr->conn, SOL_SOCKET, SO_ERROR, (char*)&err, &errLen) == 0)
            {
                fprintf(stderr, "[BT-DEBUG] Poll: exceptfds signaled! SO_ERROR=%d (%s)\n", err, FormatErrorString(err).c_str());
                ptr->lastError = FormatErrorString(err);
            }
            else
            {
                fprintf(stderr, "[BT-DEBUG] Poll: exceptfds signaled! getsockopt failed, WSA=%d\n", WSAGetLastError());
                // Fallback if getsockopt fails
                ptr->lastError = FormatErrorString(WSAGetLastError());
            }
            return MDR_RESULT_ERROR_NET;
        }

        // A non-blocking connect *succeeds* by signaling the write set.
        // Or, if already connected, data is available (read set).
        if (FD_ISSET(ptr->conn, &readfds) || FD_ISSET(ptr->conn, &writefds))
        {
            return MDR_RESULT_OK;
        }

        // Should be unreachable if res > 0, but good to be safe
        return MDR_RESULT_ERROR_TIMEOUT;
    }

    if (res == 0) // Timeout
        return MDR_RESULT_ERROR_TIMEOUT;

    // res < 0 (SOCKET_ERROR)
    ptr->lastError = FormatErrorString(WSAGetLastError());
    return MDR_RESULT_ERROR_NET;
}

    // Enumeration uses ERROR_NO_MORE_ITEMS for normal exhaustion at both levels.
    static DWORD EnumerationError() noexcept
    {
        const DWORD error = ::GetLastError();
        return error == ERROR_NO_MORE_ITEMS ? ERROR_SUCCESS : error;
    }

    static DWORD CollectRadioDevices(HANDLE radio, std::vector<MDRDeviceInfo>& devices)
    {
        BLUETOOTH_DEVICE_SEARCH_PARAMS search = {.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),
                                                  .fReturnConnected = TRUE,
                                                  .cTimeoutMultiplier = 15,
                                                  .hRadio = radio};
        BLUETOOTH_DEVICE_INFO info = {sizeof(BLUETOOTH_DEVICE_INFO)};
        HBLUETOOTH_DEVICE_FIND found = BluetoothFindFirstDevice(&search, &info);
        if (!found)
            return EnumerationError();

        do
        {
            auto const& bytes = info.Address.rgBytes;
            std::string name = ToU8String(info.szName);
            std::string address = fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                                              bytes[5], bytes[4], bytes[3], bytes[2], bytes[1], bytes[0]);
            MDRDeviceInfo device{};
            strncpy(device.szDeviceName, name.c_str(), sizeof(device.szDeviceName) - 1);
            strncpy(device.szDeviceMacAddress, address.c_str(), sizeof(device.szDeviceMacAddress) - 1);
            devices.push_back(device);
        }
        while (BluetoothFindNextDevice(found, &info));

        const DWORD error = EnumerationError();
        if (!BluetoothFindDeviceClose(found) && error == ERROR_SUCCESS)
            return ::GetLastError();
        return error;
    }

    static int GetDevicesList(void* user, MDRDeviceInfo** ppList, int* pCount) noexcept
    {
        auto* ptr = static_cast<MDRConnectionWindows*>(user);
        *ppList = nullptr;
        *pCount = 0;
        BLUETOOTH_FIND_RADIO_PARAMS params = {sizeof(BLUETOOTH_FIND_RADIO_PARAMS)};
        HANDLE radio = nullptr;
        HBLUETOOTH_RADIO_FIND found = BluetoothFindFirstRadio(&params, &radio);
        if (!found)
        {
            const DWORD error = ::GetLastError();
            if (error == ERROR_NO_MORE_ITEMS)
            {
                ptr->lastError = "No Bluetooth adapter available";
                return MDR_RESULT_ERROR_NOT_FOUND;
            }
            ptr->lastError = FormatErrorString(error);
            return MDR_RESULT_ERROR_NET;
        }

        std::vector<MDRDeviceInfo> devices;
        DWORD error;
        while (true)
        {
            error = CollectRadioDevices(radio, devices);
            if (!CloseHandle(radio) && error == ERROR_SUCCESS)
                error = ::GetLastError();
            if (error != ERROR_SUCCESS)
                break;
            if (!BluetoothFindNextRadio(found, &radio))
            {
                error = EnumerationError();
                break;
            }
        }
        if (!BluetoothFindRadioClose(found) && error == ERROR_SUCCESS)
            error = ::GetLastError();
        if (error != ERROR_SUCCESS)
        {
            ptr->lastError = FormatErrorString(error);
            return MDR_RESULT_ERROR_NET;
        }
        if (!devices.empty())
        {
            *ppList = new MDRDeviceInfo[devices.size()];
            std::memcpy(*ppList, devices.data(), devices.size() * sizeof(MDRDeviceInfo));
            *pCount = static_cast<int>(devices.size());
        }
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
        auto* ptr = static_cast<MDRConnectionWindows*>(user);
        return ptr->lastError.c_str();
    }
};

extern "C" {
MDRConnectionWindows* mdrConnectionWindowsCreate() { return new MDRConnectionWindows(); }
void mdrConnectionWindowsDestroy(MDRConnectionWindows* instance) { delete instance; }
MDRConnection* mdrConnectionWindowsGet(MDRConnectionWindows* instance) { return &instance->mdrConn; }
}
