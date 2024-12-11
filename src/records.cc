#include "logs.h"
#include "utils.h"
#include "records.h"
// 记录插件enable之后的各项计数
global_records records;

void reset_records()
{
    records.processed_samples = 0;
    records.pids.clear();
    records.modules.clear();
}

void debug_print_records()
{
    DEBUG("---------------------------------------------------------------");
    DEBUG("[DFOT_RECORD] processed_samples: " << records.processed_samples);
    DEBUG("[DFOT_RECORD] total pids       : " << records.pids.size());
    for (auto it = records.pids.begin(); it != records.pids.end(); ++it) {
        int pid = it->first;
        Pidinfo* info = it->second;
        // 过滤非目标应用的pid
        if (info->instance == nullptr || info->instance->app == nullptr) {
            continue;
        }
        DEBUG("[DFOT_RECORD]   [" <<  info->instance->app->app_name << "] pid: " << pid
            << ", starttime: " << turn_timestamp_to_format_time(info->ts)
            << ", instance version: " << info->instance->version
            << ", instance createtime: "
            << turn_timestamp_to_format_time(info->instance->id * 1000));
    }
    DEBUG("[DFOT_RECORD] the others are not target app's pids");
}
