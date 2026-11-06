#pragma once

#include <mdr-c/Connection.h>
#include <vector>
#include <string>
#include <cstdint>

int bluez_list_devices(std::vector<MDRDeviceInfo>& devices, std::string& error);

struct sdp_session;
sdp_session* sdp_connect_nb(const char* dev_addr);
int sdp_poll(sdp_session* sess, int timeout);
uint8_t sdp_getServiceChannel(sdp_session* session, const uint8_t* uuid128);
void sdp_close(sdp_session* sess);