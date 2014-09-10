
#include "kinetic_types.h"

const char* KineticStatusDescriptor[] = {
    "SUCCESS",
    "DEVICE_BUSY",
    "CONNECTION_ERROR",
    "INVALID_REQUEST",
    "OPERATION_FAILED",
    "VERSION_FAILURE",
    "DATA_ERROR",
};

const int KineticStatusDescriptorCount = sizeof(KineticStatusDescriptor)/sizeof(char*);
