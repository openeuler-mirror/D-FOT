#include <iomanip>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "utils.h"
#include "logs.h"
#include "configs.h"

GlobalConfig *configs = nullptr;

void cleanup_configs()
{
    if (configs == nullptr) {
        return;
    }
    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        delete *it;
    }
    configs->apps.clear();
    delete configs;
    configs = nullptr;
}

void debug_print_configs()
{
    if (configs == nullptr) {
        ERROR("invalid configs");
        return;
    }

    DEBUG("---------------------------------------------------------------");
    DEBUG("[DFOT_CONFIG] INI_PATH                     : "
          << configs->ini_path);
    DEBUG("[DFOT_CONFIG] COLLECTOR_SAMPLING_STRATEGY  : "
          << configs->sampling_strategy);
    DEBUG("[DFOT_CONFIG] COLLECTOR_HIGH_LOAD_THRESHOLD: "
          << configs->high_load_threshold);
    DEBUG("[DFOT_CONFIG] COLLECTOR_SAMPLING_PERIOD    : "
          << configs->collector_sampling_period);
    DEBUG("[DFOT_CONFIG] COLLECTOR_SAMPLING_FREQ      : "
          << configs->collector_sampling_freq);
    DEBUG("[DFOT_CONFIG] COLLECTOR_DATA_AGING_TIME    : "
          << configs->collector_data_aging_time);
    DEBUG("[DFOT_CONFIG] TUNER_TOOL                   : "
          << configs->tuner_tool);
    DEBUG("[DFOT_CONFIG] TUNER_CHECK_PERIOD           : "
          << configs->tuner_check_period);
    DEBUG("[DFOT_CONFIG] TUNER_PROFILE_DIR            : "
          << configs->tuner_profile_dir);
    DEBUG("[DFOT_CONFIG] TUNER_OPTIMIZING_STRATEGY    : "
          << configs->tuner_optimizing_strategy);
    DEBUG("[DFOT_CONFIG] TUNER_OPTIMIZING_CONDITION   : "
          << configs->tuner_optimizing_condition);

    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        AppConfig *app = *it;
        DEBUG("-------------------------------------------------------");
        DEBUG("[DFOT_CONFIG] APP                : " << app->app_name);
        DEBUG("[DFOT_CONFIG] FULL_PATH          : " << app->full_path);
        DEBUG("[DFOT_CONFIG] DEFAULT_PROFILE    : " << app->default_profile);
        DEBUG("[DFOT_CONFIG] DUMP_DATA_THRESHOLD: " << app->collector_dump_data_threshold);
        DEBUG("[DFOT_CONFIG] BOLT_DIR           : " << app->bolt_dir);
        DEBUG("[DFOT_CONFIG] BOLT_OPTIONS       : " << app->bolt_options);
        DEBUG("[DFOT_CONFIG] UPDATE_DEBUG_INFO  : " << app->update_debug_info);
    }
    DEBUG("---------------------------------------------------------------");
}

int parse_general(boost::property_tree::ptree pt)
{
    try {
        std::string log_level = pt.get<std::string>("general.LOG_LEVEL");
        auto it = LOG_LEVEL.find(log_level);
        if (it == LOG_LEVEL.end()) {
            ERROR("invalid log level: " << pt.get<std::string>("general.LOG_LEVEL")
                  << ", only support DEBUG|INFO|WARN|ERROR|FATAL");
            return DFOT_ERROR;
        } else {
            configs->log_level = it->second;
        }
        logger.setLogLevel(configs->log_level);
        configs->sampling_strategy             = pt.get<int>("general.COLLECTOR_SAMPLING_STRATEGY");
        configs->high_load_threshold           = pt.get<int>("general.COLLECTOR_HIGH_LOAD_THRESHOLD");
        configs->collector_sampling_period     = pt.get<int>("general.COLLECTOR_SAMPLING_PERIOD");
        configs->collector_sampling_freq       = pt.get<int>("general.COLLECTOR_SAMPLING_FREQ");
        configs->collector_data_aging_time     = pt.get<int>("general.COLLECTOR_DATA_AGING_TIME");
        configs->tuner_tool                    = pt.get<std::string>("general.TUNER_TOOL");
        configs->tuner_check_period            = pt.get<int>("general.TUNER_CHECK_PERIOD");
        configs->tuner_profile_dir             = pt.get<std::string>("general.TUNER_PROFILE_DIR");
        int strategy = pt.get<int>("general.TUNER_OPTIMIZING_STRATEGY");
        if (strategy == 0) {
            configs->tuner_optimizing_strategy = OPTIMIZE_ONE_TIME;
        } else {
            configs->tuner_optimizing_strategy = OPTIMIZE_CONTINUOUS;
        }
        configs->tuner_optimizing_condition    = pt.get<int>("general.TUNER_OPTIMIZING_CONDITION");
    } catch (const boost::property_tree::ptree_bad_path &e) {
        ERROR("Error accessing property: " << e.what());
        return DFOT_ERROR;
    }

    return DFOT_OK;
}


// profile命名方式：<app_name>_<32bit_hash>_<dump_data_threshold>.profile
// app_name: 二进制名
// 32bit_hash: 二进制绝对路径的32bit hash值
// dump_data_threshold: 收集数据阈值
std::string get_app_collected_profile_path(AppConfig *app)
{
    if (app->collected_profile == "") {
        std::stringstream ss;
        uint32_t hash = static_cast<uint32_t>(
            std::hash<std::string>{}(app->full_path) & 0xFFFFFFFF);
        ss << std::hex << std::setw(8) << std::setfill('0') << hash;
        app->collected_profile = configs->tuner_profile_dir + "/" +
            app->app_name + "_" + ss.str() + "_" +
            std::to_string(app->collector_dump_data_threshold) + ".profile";   
    }

    return app->collected_profile;
}

int parse_app(boost::property_tree::ptree pt, std::string app_name)
{
    std::string full_path;
    try {
        full_path = pt.get<std::string>(app_name + ".FULL_PATH");
        if (!boost::filesystem::exists(full_path)) {
            ERROR("Error: File does not exist: " << full_path);
            return DFOT_ERROR;
        }
    } catch (const boost::property_tree::ptree_bad_path &e) {
        ERROR("FULL_PATH is needed.");
        return DFOT_ERROR;
    }

    auto app = new AppConfig;
    if (app == nullptr) {
        ERROR("[enable] app is nullptr");
        return DFOT_ERROR;
    }

    app->full_path         = full_path;
    app->app_name          = app_name;
    app->current_pid       = INVALID_PID;
    app->profile.ts        = 0;
    app->status            = UNOPTIMIZED;
    app->collected_profile = "";
    app->bolt_options      = "";
    app->update_debug_info = false;

    try {
        app->default_profile =
            pt.get<std::string>(app_name + ".DEFAULT_PROFILE");
        // 如果有预置profile则需要检查文件是否存在
        // 没有预置profile也是正常场景
        if (app->default_profile != "") {
            if (!boost::filesystem::exists(app->default_profile)) {
                ERROR("default profile does not exist: " << app->default_profile);
                return DFOT_ERROR;
            }
            app->status = NEED_OPTIMIZED;
        }
    } catch (const boost::property_tree::ptree_bad_path &e) {
        // 没有DEFAULT_PROFILE配置项也不影响调优
        DEBUG(app_name << " has no default profile");
    }

    try {
        app->collector_dump_data_threshold = 
            pt.get<unsigned int>(app_name + ".COLLECTOR_DUMP_DATA_THRESHOLD");
    } catch (const boost::property_tree::ptree_bad_path &e) {
        ERROR(app_name << " has no valid COLLECTOR_DUMP_DATA_THRESHOLD");
        return DFOT_ERROR;
    }

    try {
        app->bolt_dir = pt.get<std::string>(app_name + ".BOLT_DIR");
        if (app->bolt_dir == "") {
            app->bolt_dir = "/usr/bin";
        }
    } catch (const boost::property_tree::ptree_bad_path &e) {
        // 没有BOLT_DIR配置项也是正常场景
        app->bolt_dir = "/usr/bin";
        DEBUG(app_name << " has no specified BOLT_DIR");
    }

    try {
        app->bolt_options =
            pt.get<std::string>(app_name + ".BOLT_OPTIONS");
        // 去掉首尾引号
        auto opts = app->bolt_options;
        if (opts.length() >= 2 && opts.front() == '"' && opts.back() == '"') {
            app->bolt_options = opts.substr(1, opts.length() - 2);
        }
    } catch (const boost::property_tree::ptree_bad_path &e) {
        // 没有定制化bolt选项也是正常场景
        DEBUG(app_name << " has no customized bolt options");
    }

    try {
        app->update_debug_info = pt.get<int>(app_name + ".UPDATE_DEBUG_INFO") == 1;
    } catch (const boost::property_tree::ptree_bad_path &e) {
        app->update_debug_info = false;
    }

    // 初始化时即确定动态收集的profile文件路径，即使本轮未导出，如果有上一轮启动留下的profile也可以复用
    app->collected_profile = get_app_collected_profile_path(app);
    configs->apps.push_back(app);

    return DFOT_OK;
}

// 解析ini文件并更新全局配置信息
int parse_dfot_ini(std::string ini_path)
{
    cleanup_configs();

    configs = new GlobalConfig;
    if (configs == nullptr) {
        ERROR("[enable] configs is nullptr");
        return DFOT_ERROR;
    }

    boost::property_tree::ptree pt;
    try {
        boost::property_tree::ini_parser::read_ini(ini_path, pt);
    } catch (const boost::property_tree::ini_parser::ini_parser_error &e) {
        ERROR("Error reading " << ini_path);
        return DFOT_ERROR;
    }
    configs->ini_path = ini_path;
    if (parse_general(pt) != DFOT_OK) {
        return DFOT_ERROR;
    }
    
    configs->apps.clear();
    for (const auto &section : pt) {
        std::string app_name = section.first;

        if (app_name == "general") {
            // 公共配置，不再处理
            continue;
        }

        if (parse_app(pt, app_name) != DFOT_OK) {
            return DFOT_ERROR;
        }
    }

    debug_print_configs();
    return DFOT_OK;
}

bool check_configs_valid()
{
    if (configs == nullptr) {
        ERROR("[enable] inited failed, please check config file: " << DEFAULT_DFOT_CONFIG_PATH);
        return false;
    }

    if (configs->apps.size() == 0) {
        WARN("[enable] no app to be tunerd, please check config file: " << DEFAULT_DFOT_CONFIG_PATH);
    }

    return true;
}
