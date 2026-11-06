#include "DBusHelper.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_set>
#include <dbus/dbus.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <sys/poll.h>

namespace {
struct BluezDevice
{
    std::string adapter, name, address;
    bool pairedOrConnected = false;
};

// Both the properties and the object/interface maps are D-Bus dictionaries.
DBusMessageIter dict_entry(DBusMessageIter array, const char*& key)
{
    DBusMessageIter entry;
    dbus_message_iter_recurse(&array, &entry);
    dbus_message_iter_get_basic(&entry, &key);
    dbus_message_iter_next(&entry);
    DBusMessageIter contents;
    dbus_message_iter_recurse(&entry, &contents);
    return contents;
}

void close_bus(DBusConnection* bus)
{
    dbus_connection_close(bus);
    dbus_connection_unref(bus);
}

void copy_name(char (&dst)[128], const std::string& text)
{
    auto size = std::min(text.size(), sizeof(dst) - 1);
    // Keep a UTF-8 character whole at the fixed C API boundary.
    while (size < text.size() && size && (static_cast<unsigned char>(text[size]) & 0xc0) == 0x80)
        --size;
    text.copy(dst, size);
    dst[size] = '\0';
}
}

int bluez_list_devices(std::vector<MDRDeviceInfo>& devices, std::string& error)
{
    DBusError detail = DBUS_ERROR_INIT;
    std::unique_ptr<DBusConnection, decltype(&close_bus)> bus(
        dbus_bus_get_private(DBUS_BUS_SYSTEM, &detail), close_bus);
    if (!bus) {
        error = detail.message ? detail.message : "Could not get system bus";
        dbus_error_free(&detail);
        return MDR_RESULT_ERROR_NOT_SUPPORTED;
    }
    dbus_connection_set_exit_on_disconnect(bus.get(), false);
    using Message = std::unique_ptr<DBusMessage, decltype(&dbus_message_unref)>;
    Message request(dbus_message_new_method_call("org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"), dbus_message_unref);
    if (!request) {
        error = "Could not allocate BlueZ request";
        return MDR_RESULT_ERROR_GENERAL;
    }
    Message reply(dbus_connection_send_with_reply_and_block(bus.get(), request.get(), 5000, &detail),
        dbus_message_unref);
    if (!reply) {
        const bool unavailable = dbus_error_has_name(&detail, DBUS_ERROR_SERVICE_UNKNOWN)
            || dbus_error_has_name(&detail, DBUS_ERROR_NAME_HAS_NO_OWNER);
        error = detail.message ? detail.message : "BlueZ discovery failed";
        dbus_error_free(&detail);
        return unavailable ? MDR_RESULT_ERROR_NOT_SUPPORTED : MDR_RESULT_ERROR_GENERAL;
    }
    if (!dbus_message_has_signature(reply.get(), "a{oa{sa{sv}}}")) {
        error = "Invalid BlueZ GetManagedObjects reply";
        return MDR_RESULT_ERROR_GENERAL;
    }

    std::unordered_set<std::string> powered;
    std::vector<BluezDevice> reported;
    DBusMessageIter body, objects;
    dbus_message_iter_init(reply.get(), &body);
    dbus_message_iter_recurse(&body, &objects);
    while (dbus_message_iter_get_arg_type(&objects) != DBUS_TYPE_INVALID) {
        const char* path;
        auto interfaces = dict_entry(objects, path);
        while (dbus_message_iter_get_arg_type(&interfaces) != DBUS_TYPE_INVALID) {
            const char* interface;
            auto properties = dict_entry(interfaces, interface);
            bool adapter = std::strcmp(interface, "org.bluez.Adapter1") == 0;
            bool device = std::strcmp(interface, "org.bluez.Device1") == 0;
            BluezDevice found;
            while ((adapter || device) && dbus_message_iter_get_arg_type(&properties) != DBUS_TYPE_INVALID) {
                const char* key;
                auto value = dict_entry(properties, key);
                int type = dbus_message_iter_get_arg_type(&value);
                if (adapter && std::strcmp(key, "Powered") == 0 && type == DBUS_TYPE_BOOLEAN) {
                    dbus_bool_t on;
                    dbus_message_iter_get_basic(&value, &on);
                    if (on) powered.insert(path);
                }
                if (device && type == DBUS_TYPE_BOOLEAN &&
                    (std::strcmp(key, "Paired") == 0 || std::strcmp(key, "Connected") == 0)) {
                    dbus_bool_t flag;
                    dbus_message_iter_get_basic(&value, &flag);
                    found.pairedOrConnected |= flag != 0;
                }
                if (device && (type == DBUS_TYPE_STRING || type == DBUS_TYPE_OBJECT_PATH)) {
                    const char* text;
                    dbus_message_iter_get_basic(&value, &text);
                    if (std::strcmp(key, "Address") == 0) found.address = text;
                    else if (std::strcmp(key, "Name") == 0) found.name = text;
                    else if (std::strcmp(key, "Adapter") == 0) found.adapter = text;
                }
                dbus_message_iter_next(&properties);
            }
            if (found.pairedOrConnected) reported.push_back(std::move(found));
            dbus_message_iter_next(&interfaces);
        }
        dbus_message_iter_next(&objects);
    }
    if (powered.empty()) {
        error = "No powered Bluetooth adapter";
        return MDR_RESULT_ERROR_NOT_FOUND;
    }
    for (const auto& entry : reported) {
        if (!powered.contains(entry.adapter) || bachk(entry.address.c_str()) != 0) continue;
        MDRDeviceInfo info{};
        copy_name(info.szDeviceName, entry.name);
        entry.address.copy(info.szDeviceMacAddress, sizeof(info.szDeviceMacAddress) - 1);
        devices.push_back(info);
    }
    return MDR_RESULT_OK;
}

sdp_session* sdp_connect_nb(const char* dev_addr)
{
    bdaddr_t target;
    str2ba(dev_addr, &target);
    /* connect to the SDP server running on the remote machine */
    const bdaddr_t bdaddr_any = {{0, 0, 0, 0, 0, 0}};
    return reinterpret_cast<sdp_session*>(sdp_connect(&bdaddr_any, &target, SDP_NON_BLOCKING));
}

int sdp_poll(sdp_session* sess, int timeout)
{
    auto session = reinterpret_cast<sdp_session_t*>(sess);
    int sock = sdp_get_socket(session);
    pollfd pfd{
        .fd = sock,
        .events = POLLIN | POLLOUT
    };
    return poll(&pfd, 1, timeout);
}

uint8_t sdp_getServiceChannel(sdp_session* sess, const uint8_t* uuid128)
{
    auto session = reinterpret_cast<sdp_session_t*>(sess);
    int status;
    uuid_t svc_uuid;
    sdp_list_t *response_list, *search_list, *attrid_list;
    uint32_t range = 0x0000ffff;
    uint8_t port = 0;

    sdp_uuid128_create(&svc_uuid, uuid128);
    search_list = sdp_list_append(0, &svc_uuid);
    attrid_list = sdp_list_append(0, &range);
    // get a list of service records that have UUID 0xabcd
    response_list = NULL;
    status = sdp_service_search_attr_req(session, search_list,
                                         SDP_ATTR_REQ_RANGE, attrid_list, &response_list);
    if (status == 0)
    {
        sdp_list_t* proto_list = NULL;
        sdp_list_t* r = response_list;

        // go through each of the service records
        for (; r; r = r->next)
        {
            sdp_record_t* rec = (sdp_record_t*)r->data;

            // get a list of the protocol sequences
            if (sdp_get_access_protos(rec, &proto_list) == 0)
            {
                // get the RFCOMM port number
                port = sdp_get_proto_port(proto_list, RFCOMM_UUID);

                sdp_list_free(proto_list, 0);
            }
            sdp_record_free(rec);
        }
    }
    sdp_list_free(response_list, 0);
    sdp_list_free(search_list, 0);
    sdp_list_free(attrid_list, 0);
    return port;
}

void sdp_close(sdp_session* sess)
{
    auto session = reinterpret_cast<sdp_session_t*>(sess);
    sdp_close(session);
}