#pragma once
#include "../Connection.h"

typedef struct MDRConnectionMacOS MDRConnectionMacOS;

#ifdef __cplusplus
extern "C" {
#endif
MDRConnectionMacOS* mdrConnectionMacOSCreate();
MDRConnection* mdrConnectionMacOSGet(MDRConnectionMacOS*);
void mdrConnectionMacOSDestroy(MDRConnectionMacOS*);
#ifdef __cplusplus
}
#endif
