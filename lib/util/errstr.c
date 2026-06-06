#include "errstr.h"

const char *agent_errstr(int err)
{
    switch (err) {
        case 0:  return "ok";
        case -1: return "error";
        case -2: return "not found";
        case -3: return "bad argument";
        case -4: return "io error";
        case -5: return "out of memory";
        default: return "unknown";
    }
}
