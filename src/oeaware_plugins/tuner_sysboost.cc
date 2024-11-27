#include <cstring>

#include "logs.h"
#include "utils.h"
#include "configs.h"
#include "records.h"
#include "interface.h"

#include "opt.h"

// 当前优化插件需要的采样数据来源于oeaware-collector采样实例PMU_CYCLES_SAMPLING
// 本插件不显式依赖该PMU_CYCLES_SAMPLING（因为可以预置profile来优化）
// 注意如果oeaware-collector仓库对应采样实例名字有变化时，此处也要同步修改
#define PMU_CYCLES_SAMPLING "pmu_cycles_sampling"
// sysboost优化插件实例名
#define TUNER_INSTANCE_NAME "dfot_tuner_sysboost"

// 从collector获取ringbuf
void get_collector_ringbuf(
    const struct Param *param, const struct DataRingBuf **ringbuf, uint64_t *cnt)
{
    const struct DataRingBuf *buf = nullptr;
    static int last_record_index = -1;     // 记录上一次处理的ring_bufs下标
    static uint64_t last_record_count = 0; // 记录上一次处理时采集插件的运行次数

    *ringbuf = nullptr;
    *cnt = 0;

    // 如果插件无变化，可以快速找到对应param
    if (last_record_index >= 0 && last_record_index < param->len &&
        param->ring_bufs[last_record_index] != nullptr &&
        strcmp(param->ring_bufs[last_record_index]->instance_name, PMU_CYCLES_SAMPLING) == 0) {
        buf = param->ring_bufs[last_record_index];
    } else {
        for (int i = 0; i < param->len; i++) {
            if (param->ring_bufs[i] == nullptr) {
                continue;
            }
            if (strcmp(param->ring_bufs[i]->instance_name, PMU_CYCLES_SAMPLING) == 0) {
                buf = param->ring_bufs[i];
                last_record_index = i;
                break;
            }
        }
    }

    if (buf == nullptr) {
        last_record_index = -1;
        last_record_count = 0;
        return;
    }

    if (buf->count > last_record_count) {
        // 数据有更新，注意DataBuf已经全部刷新的场景
        *ringbuf = buf;
        *cnt = std::min(buf->count - last_record_count, (uint64_t)buf->buf_len);
        last_record_count = buf->count;
    } else if (buf->count < last_record_count) {
        // 异常场景
        WARN("[run] record data count: " << last_record_count
            << " is large than " << "current count: " << buf->count);
        *ringbuf = nullptr;
        last_record_count = 0;
    }
}

void get_sampling_data_from_collector(const struct Param *param)
{
    const struct DataRingBuf *ringbuf = nullptr;
    uint64_t cnt = 0; // 需要处理的DataBuf的个数，小于buf_len

    int64_t start_ts = get_current_timestamp();

    get_collector_ringbuf(param, &ringbuf, &cnt);
    if (ringbuf == nullptr || cnt == 0) {
        return;
    }

    // 从ringbuf->index开始，倒序处理cnt个DataBuf，同时校验PmuData的ts；
    uint64_t total_samples = 0;
    for (int i = 0; i < (int)cnt; i++) {
        int index = (ringbuf->buf_len + ringbuf->index - i) % ringbuf->buf_len;
        process_pmudata((struct PmuData *)(ringbuf->buf[index].data), ringbuf->buf[index].len);
        total_samples += (uint64_t)ringbuf->buf[index].len;
    }
    records.processed_samples += total_samples;

    int64_t end_ts = get_current_timestamp();
    DEBUG("[run] processing pmudata cost: " << (end_ts - start_ts) << " ms, "
        << "current: " << total_samples << " samples, "
        << "total: " << records.processed_samples << " samples");
}

const char *sysboost_get_version()
{
    return "v1.0";
}

const char *sysboost_get_name()
{
    return TUNER_INSTANCE_NAME;
}

const char *sysboost_get_description()
{
    return "dfot tuner: sysboost";
}

const char *sysboost_get_dep()
{
    // 本插件启动时，不依赖采样插件，兼容使用预置profile的场景
    // 本插件启动后，增加对PMU_CYCLES_SAMPLING的依赖，方便获取ringbuf数据
    // configs非空即表示插件已启动
    return configs != nullptr ? PMU_CYCLES_SAMPLING : nullptr;
}

int sysboost_get_priority()
{
    return 0;
}

int sysboost_get_type()
{
    return -1;
}

// 每隔多少ms执行一次run
int sysboost_get_period()
{
    return configs != nullptr ? configs->tuner_check_period : 1000;
}

bool sysboost_enable()
{
    logger.init();

    if (configs == nullptr &&
        parse_dfot_ini(DEFAULT_DFOT_CONFIG_PATH) != DFOT_OK) {
        ERROR("[enable] instance [" << TUNER_INSTANCE_NAME << "] init configs failed");
        return false;
    }

    if (!check_configs_valid()) {
        ERROR("[enable] invalid configs");
        return false;
    }

    if (!check_dependence_ready()) {
        ERROR("[enable] dependencies are not ready");
        return false;
    }

    reset_records();

    INFO("[enable] plugin instance [" << TUNER_INSTANCE_NAME << "] enabled");
	return true;
}

void sysboost_disable()
{
    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        AppConfig *app = *it;
        if (app->status != OPTIMIZED) {
            continue;
        }
        exec_cmd("sysboostd --stop=" + app->full_path);
    }

    cleanup_configs();
    INFO("[disable] plugin " << TUNER_INSTANCE_NAME << " disabled");
}

const struct DataRingBuf *sysboost_get_ring_buf()
{
    // 调优插件不需要向其他插件提供数据
    return nullptr;
}

void sysboost_run(const struct Param *param)
{
    // 1. 刷新采样数据
    // 2. 检查优化条件
    // 3. 获取profile，实施优化
    static bool optimizing = false;

    if (configs == nullptr) {
        FATAL("[run] no valid configs found");
        return;
    }

    // 防止第一轮优化还未结束就触发新一轮优化
    if (optimizing) {
        DEBUG("[run] last optimizing is not finished, skip");
        return;
    }

    optimizing = true;

    // step1: 刷新采样数据
    get_sampling_data_from_collector(param);

    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        AppConfig *app = *it;
        // step2: 检查应用是否满足优化条件
        if (!is_app_eligible_for_optimization(app)) {
            continue;
        }

        // step3: 获取profile文件并优化
        std::lock_guard<std::mutex> lock(app->profile_mtx);
        std::string profile = get_app_profile(app);
        if (profile == "") {
            continue;
        }
        do_optimize(app, profile);
    }

    optimizing = false;
}

struct Interface sysboost_tuner = {
    .get_version     = sysboost_get_version,
    .get_name        = sysboost_get_name,
    .get_description = sysboost_get_description,
    .get_dep         = sysboost_get_dep,
    .get_priority    = sysboost_get_priority,
    .get_type        = sysboost_get_type,
    .get_period      = sysboost_get_period,
    .enable          = sysboost_enable,
    .disable         = sysboost_disable,
    .get_ring_buf    = sysboost_get_ring_buf,
    .run             = sysboost_run,
};