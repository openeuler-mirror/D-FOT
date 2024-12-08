#include <cstring>
#include <securec.h>

#include <oeaware/interface.h>
#include <oeaware/data_list.h>
#include <oeaware/data/pmu_sampling_data.h>

#include "logs.h"
#include "utils.h"
#include "configs.h"
#include "records.h"

#include "opt.h"
#include "tuner.h"

// 当前优化插件需要的采样数据来源于oeaware-manager采样实例pmu_sampling_collector
// 本插件通过订阅获取pmu_sampling_collector的采样数据，也可以预置profile来优化
// 注意如果oeaware-manager仓库对应采样实例名字有变化时，此处也要同步修改
#define DEP_INSTANCE_NAME OE_PMU_SAMPLING_COLLECTOR
// 订阅性能事件
#define DEP_TOPIC_NAME "cycles"
// sysboost优化插件实例名
#define TUNER_INSTANCE_NAME "dfot_tuner_sysboost"

SysboostTuner::SysboostTuner()
{
    // 基类参数
    name = TUNER_INSTANCE_NAME;
    version = "1.0.0";
    description = "dfot tuner: sysboost";
    priority = 2;
    type = oeaware::TUNE;
    period = 1000;

    depTopic.instanceName = DEP_INSTANCE_NAME;
    depTopic.topicName = DEP_TOPIC_NAME;

    processingArea = nullptr;
    processingAreaSize = 0;
}

SysboostTuner::~SysboostTuner()
{
    if (processingArea != nullptr) {
        free(processingArea);
        processingArea = nullptr;
        processingAreaSize = 0;
    }
}

/// @brief 调优插件不需要打开topic
/// @param topic 
/// @return 
oeaware::Result SysboostTuner::OpenTopic(const oeaware::Topic &topic)
{
    (void)topic;
    return oeaware::Result(OK);
}

/// @brief 调优插件不需要关闭topic
/// @param topic 
void SysboostTuner::CloseTopic(const oeaware::Topic &topic)
{
    (void)topic;
}

/// @brief 处理依赖采集插件实例的新采样数据
/// @param dataList 采样数据
void SysboostTuner::UpdateData(const DataList &dataList)
{
    if (configs == nullptr) {
        FATAL("[update] no valid configs found");
        return;
    }

    static bool processing = false;
    if (processing) {
        DEBUG("[update] last processing is not finished, skip");
        return;
    }
    processing = true;
    
    int64_t start_ts = get_current_timestamp();
    uint64_t total_samples = 0;
    for (unsigned long long i = 0; i < dataList.len; i++) {
        PmuSamplingData *data = (PmuSamplingData *)(dataList.data[i]);

        // 复制一份采样数据到插件公共内存中，防止数据被覆盖，
        // 如果内存不足，则重新申请内存
        if (processingArea == nullptr ||
            processingAreaSize < sizeof(PmuData) * data->len) {
            if (processingArea != nullptr) {
                free(processingArea);
            }
            processingArea = malloc(sizeof(PmuData) * data->len);
            if (processingArea == nullptr) {
                processingAreaSize = 0;
                continue;
            }
            processingAreaSize = sizeof(PmuData) * data->len;
        }
        auto ret = memcpy_s(
            processingArea, processingAreaSize, data->pmuData, sizeof(PmuData) * data->len);
        if (ret != EOK) {
            continue;
        }
        process_pmudata((PmuData *)processingArea, data->len);
        total_samples += data->len;
    }
    records.processed_samples += total_samples;

    int64_t end_ts = get_current_timestamp();
    DEBUG("[update] processing pmudata cost: " << (end_ts - start_ts) << " ms, "
        << "current: " << total_samples << " samples, "
        << "total: " << records.processed_samples << " samples");
    processing = false;
}

/// @brief 使能调优插件实例
/// @param param 预留参数
/// @return 
oeaware::Result SysboostTuner::Enable(const std::string &param)
{
    (void)param;
    
    dfot_logger.init();

    if (configs == nullptr &&
        parse_dfot_ini(DEFAULT_DFOT_CONFIG_PATH) != DFOT_OK) {
        ERROR("[enable] instance [" << TUNER_INSTANCE_NAME << "] init configs failed");
        return oeaware::Result(FAILED);
    }

    if (!check_configs_valid()) {
        ERROR("[enable] invalid configs");
        return oeaware::Result(FAILED);
    }

    if (!check_dependence_ready()) {
        ERROR("[enable] dependencies are not ready");
        return oeaware::Result(FAILED);
    }

    reset_records();

    if (Subscribe(depTopic).code != OK) {
        ERROR("[enable] subscribe dep topic error");
        return oeaware::Result(FAILED);
    }

    INFO("[enable] plugin instance [" << TUNER_INSTANCE_NAME << "] enabled");
	return oeaware::Result(OK);
}

/// @brief 禁用调优插件实例
void SysboostTuner::Disable()
{
    if (Unsubscribe(depTopic).code != OK) {
        ERROR("[disable] unsubscribe dep topic error");
    }

    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        AppConfig *app = *it;
        if (app->status != OPTIMIZED) {
            continue;
        }
        auto result = exec_cmd("sysboostd --stop=" + app->full_path);
        if (result.ret != 0) {
            ERROR("[disable] cleanup last optimization for [" << app->app_name << "] failed!");
        }
    }

    cleanup_configs();
    INFO("[disable] instance [" << TUNER_INSTANCE_NAME << "] disabled");
}

/// @brief 调优插件主逻辑
void SysboostTuner::Run()
{
    // 1. 检查优化条件
    // 2. 获取profile，实施优化
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
            // 无法匹配profile文件时，需要回退app的NEED_OPTIMIZED状态，避免重复判断和日志打印
            app->status = (app->instances.size() > 1) ? OPTIMIZED : UNOPTIMIZED;
            continue;
        }
        do_optimize(app, profile);
    }

    optimizing = false;
}
