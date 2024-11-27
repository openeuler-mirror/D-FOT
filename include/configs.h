/******************************************************************************
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * oeAware is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 ******************************************************************************/ 
#ifndef __CONFIGS_H__
#define __CONFIGS_H__

#include <string>
#include <map>
#include <mutex>

#include "logs.h"

#define DEFAULT_DFOT_CONFIG_PATH "/etc/dfot/dfot.ini"

typedef struct {
    std::string name;
    unsigned long offset;
    int count;
} AddrInfo;

typedef struct {
    int64_t ts;
    std::map<unsigned long, AddrInfo> addrs;
    std::map<std::string, std::map<unsigned long, int>> funcs;
} Profile;

enum APP_STATUS {
    UNOPTIMIZED,    // 未优化状态
    NEED_OPTIMIZED, // 待优化状态，优先使用动态profile，其次开箱profile
    OPTIMIZED       // 已优化状态
};

typedef struct BinaryInstance BinaryInstance;

typedef struct AppConfig {
    std::string app_name;
    std::string full_path;
    int         current_pid;        // app当前运行进程的pid
    char        build_id[20];       // app二进制对应的buildid，用于校验采样对象和优化对象是否一致

    Profile      profile;           // app对应profile数据
    std::string  collected_profile; // app对应profile文件路径
    std::mutex   profile_mtx;       // collector和tuner操作profile的互斥锁

    std::string  default_profile;   // 开箱profile
    unsigned int collector_dump_data_threshold;
    APP_STATUS   status;
    std::string  bolt_dir;
    std::string  bolt_options;
    bool         update_debug_info;
    std::vector<BinaryInstance *> instances;
} AppConfig;

struct BinaryInstance {
    AppConfig *app;       // 优化实例对应的应用信息
    unsigned int version;       // 优化版本标记，0表示未优化的原始版本，1表示第一次优化版本，以此类推
    std::string full_path; // 优化实例的二进制路径
    int64_t id;            // 优化实例的区分标记，当前暂时使用create_time
};

enum TUNER_OPTIMIZING_STRATEGY {
    OPTIMIZE_ONE_TIME = 0,
    OPTIMIZE_CONTINUOUS = 1
};

typedef struct {
    std::string ini_path;

    log4cplus::LogLevel log_level;
    int sampling_strategy;
    int high_load_threshold;
    int collector_sampling_period;
    int collector_sampling_freq;
    int collector_data_aging_time;
    std::string tuner_tool; 
    int tuner_check_period;
    std::string tuner_profile_dir;
    TUNER_OPTIMIZING_STRATEGY tuner_optimizing_strategy;
    int tuner_optimizing_condition;

    std::vector<AppConfig *> apps;
} GlobalConfig;

extern GlobalConfig *configs;
extern void cleanup_configs();
extern void debug_print_configs();

extern int parse_dfot_ini(std::string ini_path);
extern bool check_configs_valid();

#endif
