#include <oeaware/interface/interface.h>

#include "logs.h"
#include "tuner.h"

Logger __attribute__((visibility("hidden"))) dfot_logger("D-FOT");

extern "C" void GetInstance(std::vector<std::shared_ptr<oeaware::Interface>> &interface)
{
    interface.emplace_back(std::make_shared<SysboostTuner>());
}
