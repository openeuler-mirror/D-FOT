#include "logs.h"

#include "interface.h"
#include "tuner.h"

Logger __attribute__((visibility("hidden"))) logger("D-FOT");

extern "C" int get_instance(struct Interface **interface)
{
    *interface = &sysboost_tuner;
    return 1;
}
