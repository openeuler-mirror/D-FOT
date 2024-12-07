#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/filesystem.hpp>

#include <libkperf/pmu.h>
#include <libkperf/symbol.h>

#include "utils.h"
#include "configs.h"
#include "records.h"

const std::string addrs_file = "/etc/dfot/addrs.txt";

// 依赖项检查：sysboost/llvm-bolt
bool check_dependence_ready()
{
    // 检查sysboost服务是否启动
    auto result = exec_cmd("systemctl is-active sysboost");
    if (result.cmd_log != "active\n") {
        ERROR("[enable] invalid sysboost status: " << result.cmd_log);
        return false;
    }

    // 检查llvm-bolt、perf2bolt是否安装
    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        AppConfig *app = *it;
        result = exec_cmd("ls -l " + app->bolt_dir + "/llvm-bolt > /dev/null 2>&1");
        if (result.ret != 0) {
            ERROR("[enable] " << app->bolt_dir << "/llvm-bolt is not installed");
            return false;
        }
        result = exec_cmd("ls -l " + app->bolt_dir + "/perf2bolt > /dev/null 2>&1");
        if (result.ret != 0) {
            ERROR("[enable] " << app->bolt_dir << "/perf2bolt is not installed");
            return false;
        }
    }

    return true;
}

std::string get_app_profile(AppConfig *app)
{
    // 使用最新的采样数据进行优化
    if (app->collected_profile != "" && boost::filesystem::exists(app->collected_profile)) {
        DEBUG("[run] using the latest collected profile: " << app->collected_profile);
        return app->collected_profile;
    }
    // 使用预置的采样数据进行优化
    if (app->default_profile != "" && boost::filesystem::exists(app->default_profile)) {
        DEBUG("[run] using the default profile: " << app->default_profile);
        return app->default_profile;
    }
    
    ERROR("[run] no valid profile found for " << app->app_name);
    return "";
}

// 清空内存中的profile数据
void clear_app_profile_data(AppConfig *app) {
    app->profile.addrs.clear();
    app->profile.funcs.clear();
    app->profile.ts = 0;
}

void update_app_profile_data(AppConfig *app, struct PmuData &data)
{
    if (data.ts < app->profile.ts) {
        // 场景1: 采样数据时间戳异常，大概率数据处理慢导致，直接丢弃
        DEBUG("[run] wrong timestamp of pmudata, data.ts: "
            << data.ts << ", app.ts(already stored in memory): " << app->profile.ts);
        return;
    } else if (app->profile.ts == 0) {
        // 场景2: 内存中没有profile数据，更新时间戳
        app->profile.ts = data.ts;
    } else if (data.ts - app->profile.ts > configs->collector_data_aging_time) {
        // 场景3: 超过老化时间，丢弃历史数据
        clear_app_profile_data(app);
        app->profile.ts = data.ts;
        DEBUG("[run] clear old profile data for " << app->app_name);
    }

    // {内存地址addr: {函数名name, 偏移offset, 计数count}, ...}
    auto &addrs = app->profile.addrs;
    // {函数名func: {内存地址addr: 计数count, ...}, ...}
    auto &funcs = app->profile.funcs;

    // symbol->addr symbol->offset
    // 如果是BOLT优化过后的二进制的采样数据则只需记录地址和计数
    unsigned long addr = data.stack->symbol->addr;

    if (records.pids[data.pid]->instance->version > 0) {
        if (addrs.find(addr) != addrs.end()) {
            addrs[addr].count++;
        } else {
            addrs[addr] = AddrInfo{std::string(""), 0, 1};
        }
        return;
    }

    // 原始二进制的采样数据，读取地址+符号+偏移
    if (addrs.find(addr) != addrs.end()) {
        addrs[addr].count++;
        funcs[addrs[addr].name][data.stack->symbol->offset]++;
    } else {
        addrs[addr] = AddrInfo();
        if (data.stack->symbol->mangleName != nullptr) {
            addrs[addr].name = data.stack->symbol->mangleName;
        } else {
            auto sym = SymResolverMapAddr(data.pid, addr);
            addrs[addr].name = sym->mangleName;
        }
        addrs[addr].offset = data.stack->symbol->offset;
        addrs[addr].count = 1;
        funcs[addrs[addr].name][data.stack->symbol->offset] = 1;
    }
}

// 获取profile中的函数数量，函数+偏移数量，以及有效sample数
void get_profile_func_offset_samples(const Profile &profile,
    int *funcs, int *offsets, int *samples)
{
    *funcs = profile.funcs.size();
    *offsets = 0;
    *samples = 0;

    for (const auto& [func_name, offset_map] : profile.funcs) {
        *offsets += offset_map.size();
        for (const auto& [offset, count] : offset_map) {
            *samples += count;
        }
    }
}

// 判断是否需要将profile数据导出到文件
bool need_flush_app_profile_to_file(AppConfig *app)
{
    // 当前仅根据地址数量判断
    return app->profile.addrs.size() >= app->collector_dump_data_threshold;
}


// 将热点地址和计数数据导出到文件（用于已被BOLT优化的二进制采样信息分析）
int dump_app_addrs_to_file(AppConfig *app)
{
    INFO("[run] dump addrs data to " << addrs_file);
    FILE *fp = fopen(addrs_file.c_str(), "w");
    if (fp == nullptr) {
        ERROR("[run] fopen " << addrs_file << " error");
        return DFOT_ERROR;
    }

    // 当前仅处理pmu_sampling_collector数据，性能事件固定为cycles
    fprintf(fp, "cycles\n");
    for (auto it = app->profile.addrs.begin(); it != app->profile.addrs.end(); ++it) {
        fprintf(fp, "%lx %d\n", it->first, it->second.count);
    }
    fclose(fp);
    return DFOT_OK;
}

// 将地址数据转换成profile，并删除第一行
 int convert_addrs_to_profile(AppConfig *app)
 {
    // 1. 使用perf2bolt转换地址数据为profile
    std::string perf2bolt_cmd = app->bolt_dir + "/perf2bolt" + " -nl" +
        " -p " + addrs_file + " --libkperf" +
        " -o " + app->collected_profile +
        " " + app->instances[app->instances.size() - 1]->full_path;
    exec_result result = exec_cmd(perf2bolt_cmd);
    if (result.ret != 0) {
        ERROR("[run] exec " << perf2bolt_cmd << " error!"
            "\nerror log: " << result.cmd_log);
        return DFOT_ERROR;
    }

    // 2. 转换文件的第一行是固定内容"boltedcollection"，需要删除
    std::ifstream inputFile(app->collected_profile);
    if (!inputFile.is_open()) {
        ERROR("[run] modified " << app->collected_profile << " error");
        std::remove(app->collected_profile.c_str());
        return DFOT_ERROR;
    }

    std::string firstLine;
    std::getline(inputFile, firstLine);

    if (firstLine == "boltedcollection") {
        std::string remainingContent;
        std::string line;

        // 读取剩余内容
        while (std::getline(inputFile, line)) {
            remainingContent += line + '\n';
        }

        inputFile.close();

        // 重新写入文件，覆盖原内容
        std::ofstream outputFile(app->collected_profile);
        if (!outputFile.is_open()) {
            ERROR("[run] modified " << app->collected_profile << " error");
            std::remove(app->collected_profile.c_str());
            return DFOT_ERROR;
        }
        outputFile << remainingContent;
        outputFile.close();
    } else {
        ERROR("[run] The content of " << app->collected_profile << " does not meet expectations.");
        std::remove(app->collected_profile.c_str());
        return DFOT_ERROR;
    }
    return DFOT_OK;
}

// 将profile数据导出到文件
void dump_app_profile_to_file(AppConfig *app)
{
    if (configs->tuner_optimizing_strategy == OPTIMIZE_ONE_TIME
        && app->status == OPTIMIZED) {
        clear_app_profile_data(app);
        return;
    }

    INFO("[run] app [" << app->app_name << "] is dumping new profile...");
    INFO("profile info:");
    INFO("- Location: " << app->collected_profile);
    auto seconds = (get_current_timestamp() - app->profile.ts) / 1000;
    INFO("- Time    : " << seconds << "s"
        << " [" << turn_timestamp_to_format_time(app->profile.ts)
        << " - " << turn_timestamp_to_format_time(get_current_timestamp()) << "]");
    INFO("- Count   : " << app->profile.addrs.size());
    
    std::lock_guard<std::mutex> lock(app->profile_mtx);

    // 二进制已经优化过，导出地址数据并通过perf2bolt转换生成profile
    if (app->instances.size() > 1) {
        if (dump_app_addrs_to_file(app) != DFOT_OK) {
            ERROR("[run] dump addrs data to file error.");
            return;
        }
        if (convert_addrs_to_profile(app) != DFOT_OK) {
            ERROR("[run] convert addrs to profile error.");
            return;
        }
    } else {
        // DEBUG模式下导出地址用于后续分析
        if (configs->log_level == log4cplus::DEBUG_LOG_LEVEL && dump_app_addrs_to_file(app) != DFOT_OK) {
            ERROR("[run] dump addrs data to file error.");
            return;
        }

        // 二进制未优化过，导出profile数据
        FILE *fp = fopen(app->collected_profile.c_str(), "w");
        if (fp == nullptr) {
            ERROR("[run] fopen " << app->collected_profile << " error");
            return;
        }
        // 当前仅处理pmu_sampling_collector数据，性能事件固定为cycles
        fprintf(fp, "no_lbr cycles:\n");
        for (auto it1 = app->profile.funcs.begin(); it1 != app->profile.funcs.end(); ++it1) {
            for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
                fprintf(fp, "1 %s %lx %d\n", it1->first.c_str(), it2->first, it2->second);
            }
        }
        fclose(fp);
    }

    // 更新app状态
    if ((configs->tuner_optimizing_strategy == OPTIMIZE_ONE_TIME
            && app->status != OPTIMIZED) ||
        configs->tuner_optimizing_strategy == OPTIMIZE_CONTINUOUS) {
        app->status = NEED_OPTIMIZED;
    }

    clear_app_profile_data(app);
}

// 根据pid获取对应的binaryinstance
BinaryInstance *find_or_create_binary_instance(AppConfig *app, pid_t pid)
{
    bool is_optimized = false;
    std::string full_path = get_bin_full_path_by_pid(pid);

    // 判断app是否是优化版本
    const std::string suffix = ".rto";
    if (full_path.size() > suffix.size() &&
        full_path.compare(full_path.size() - suffix.size(), suffix.size(), suffix) == 0) {
        is_optimized = true;
    }

    time_t ctime = get_file_create_time(full_path);

    // 非优化版本，不能存在优化版本采样数据
    // 1. 正常场景：无采样数据和实例 -- 创建新实例
    // 2. 异常场景：无实例但有采样数据 -- 丢弃采样数据
    // 3. 异常场景：有优化实例但当前采样数据指向的是非优化版本 -- 异常场景
    // 4. 异常场景：有多个实例但当前采样数据指向的是非优化版本 -- 异常场景
    if (!is_optimized) {
        if (app->instances.size() == 0 && app->profile.addrs.size() == 0) {
            app->instances.push_back(new BinaryInstance{app, 0, full_path, ctime});
        } else if (app->instances.size() == 0 && app->profile.addrs.size() != 0) {
            ERROR("[run] found data remnants for app: " << app->app_name);
            clear_app_profile_data(app);
            return nullptr;
        } else if (app->instances.size() == 1
            && app->instances[0]->full_path != full_path) {
            ERROR("[run] mismatch instance for app: " << app->app_name);
            return nullptr;
        } else if (app->instances.size() > 1) {
            ERROR("[run] multiple instances for app: " << app->app_name);
            return nullptr;
        }
        return app->instances[0];
    }

    // 优化版本
    // 1. 正常场景：有实例且实例id与当前二进制创建时间一致 -- 返回实例
    // 2. 正常场景：无实例 -- 创建新实例
    if (app->instances.size() > 0
        && app->instances[app->instances.size() - 1]->id == ctime) {
        return app->instances[app->instances.size() - 1];
    } else if (app->instances.size() == 0) {
        // 直接通过预置profile优化的场景，补充一个未优化实例，方便使用no判断优化
        char rlpath[1024] = {0};
        get_real_path(app->full_path.c_str(), rlpath);
        app->instances.push_back(new BinaryInstance{app, 0, std::string(rlpath), 0});
    }

    clear_app_profile_data(app);
    app->instances.push_back(new BinaryInstance{
        app, (unsigned int)app->instances.size(), full_path, ctime});
    return app->instances[app->instances.size() - 1];
}

// 获取采样数据对应的app，并创建对应的binaryinstance等数据缓存
// 区分目标应用/目标应用优化版本/非目标应用
AppConfig *get_app_and_build_data_cache(struct PmuData *data)
{
    // 如果该数据对应的pid已经记录过，则无需再判断二进制信息
    // 根据instance判断是否是目标应用
    if (records.pids.find(data->pid) != records.pids.end()) {
        records.pids[data->pid]->ts = data->ts;
        if (records.pids[data->pid]->instance == nullptr) { // 非目标应用，返回空app
            return nullptr;
        }
        return records.pids[data->pid]->instance->app; // 目标应用，返回对应app，用于记录profile信息
    }

    unsigned int index = 0;
    for (; index < configs->apps.size(); ++index) {
        if (strcmp(configs->apps[index]->app_name.c_str(), data->comm) == 0) {
            break;
        }
    }
    // 非目标应用，创建空pidinfo
    if (index == configs->apps.size()) {
        records.pids[data->pid] = new Pidinfo{nullptr, data->pid, data->ts};
        return nullptr;
    }

    BinaryInstance *bi = find_or_create_binary_instance(configs->apps[index], data->pid);
    if (bi == nullptr) {
        ERROR("[run] find or create binary instance for [" << configs->apps[index]->app_name << "] failed");
        return nullptr;
    }
    records.pids[data->pid] = new Pidinfo{bi, data->pid, data->ts};
    return configs->apps[index];
}

// 处理pmu采样数据
void process_pmudata(struct PmuData *data, int len)
{
    // 1. 根据data中的pid判断app
    // 2. 判断现存数据的ts，如果ts在老化时间阈值前，则丢弃历史数据，并处理当前数据，刷新ts；如果ts未到阈值则处理当前数据
    // 3. 数据处理完成后，如果现存数据可以有效优化（足够多），则导出数据作为最新profile，导出后丢弃内存中的数据，并置needtuned为true
    // app->profile内部结构：
    // {
    //     最早一条profile数据的timestamp
    //     int64_t ts;
    //     地址信息，减少符号解析过程
    //     std::map<unsigned long, AddrInfo> addrs;
    //       {<addr>: {<name>,<offset>,<count>}, ...}
    //     函数信息，编译统计和导出
    //     std::map<std::string, std::map<unsigned long, int>> funcs;
    //       {<name>: {<offset>: <count>, ...}, ...}
    // }

    std::set<AppConfig*> updated_apps;

    for (int i = 0; i < len; ++i) {
        AppConfig *app = get_app_and_build_data_cache(&data[i]);

        if (app == nullptr ||                   // 未匹配到app，直接跳过
            data[i].stack == nullptr ||         // 空数据，直接跳过
            data[i].stack->symbol == nullptr) { // 空数据，直接跳过
            continue;
        }

        update_app_profile_data(app, data[i]);
        updated_apps.insert(app);
    }

    for (AppConfig* app : updated_apps) {
        DEBUG("[update] collected addrs for [" << app->app_name
            << ": " << app->instances.size() - 1 << "]: "
            << app->profile.addrs.size());

        // 导出bolt profile（函数名+偏移+计数）
        if (!need_flush_app_profile_to_file(app)) {
            continue;
        }
        dump_app_profile_to_file(app);
    }
}

// 调用sysboostd进行优化
void do_optimize(AppConfig *app, std::string profile)
{
    const std::string required_bolt_options = "--enable-bat";
    const std::string debug_bolt_options    = "-update-debug-sections";
    const std::string default_bolt_options  =
        "-reorder-blocks=ext-tsp -reorder-functions=hfsort+ "
        "-split-functions -split-all-cold -icf=1 "
        "-use-gnu-stack --inline-all";

    INFO("[run] try to optimize app [" << app->app_name << "] "
        "with profile [" << profile << "]");

    // 构造并执行sysboost优化回退命令（无论是否优化过）
    auto result = exec_cmd("sysboostd --stop=" + app->full_path);
    if (result.ret != 0) {
        ERROR("[run] cleanup last optimization for [" << app->app_name << "] failed!");
        return;
    }

    // 构造并执行sysboost优化使能命令
    std::string bolt_options =
        app->bolt_options == "" ? default_bolt_options : app->bolt_options;
    if (bolt_options.find(required_bolt_options) == std::string::npos) {
        bolt_options += " " + required_bolt_options;
    }
    if (app->update_debug_info && bolt_options.find(debug_bolt_options) == std::string::npos) {
        bolt_options += " " + debug_bolt_options;
    }

    std::string opt_cmd = std::string("sysboostd") +
        " --gen-bolt=" + app->full_path +
        " --bolt-option=\"" + bolt_options + "\"" +
        " --profile-path=" + profile;

    uint64_t start_ts = get_current_timestamp();
    result = exec_cmd(opt_cmd);
    uint64_t end_ts = get_current_timestamp();
    if (result.ret != 0) {
        ERROR("[run] optimizing failed, please check the sysboost log");
    } else {
        DEBUG("[run] optimizing finished, cost: " << (end_ts - start_ts)/1000 << " s");
        app->status = OPTIMIZED;
    }

    // 优化后需要清除当前profile数据，避免拉起优化二进制前后的数据混合
    clear_app_profile_data(app);
}

// full_path为空时，如果有多个同名进程，返回获取到的第一个pid
// full_path不为空，则获取到pid后需要校验路径
int get_target_pid (AppConfig *app)
{
    char buffer[1024] = {0};
    std::string command = "pidof " + app->app_name;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        WARN("get " << app->app_name << "'s pid failed!");
        return -1;
    }
    
    // 获取每一个pid进行判断
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        int pid = atoi(buffer);
        std::string proc_path = "/proc/" + std::to_string(pid) + "/exe";
        if (!get_real_path(proc_path.c_str(), buffer)) {
            WARN("get realpath of pid:" << pid << " failed!");
            continue;
        }

        for (unsigned int index = 0; index < app->instances.size(); ++index) {
            if (std::string(buffer) == app->instances[index]->full_path) {
                return pid;
            }
        }
    }
    return -1;
}

// 判断应用是否满足优化条件
bool is_app_eligible_for_optimization(AppConfig *app)
{
    if (app->status != NEED_OPTIMIZED) {
        return false;
    }

    if (configs->tuner_optimizing_condition == 0) {
        if (get_target_pid(app) > 0) {
            return false;
        }
        return true;
    }

    return false;
}

std::vector<int> update_pid_in_configs()
{
    std::vector<int> pids;
    if (configs == nullptr) {
        return pids;
    }

    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        AppConfig *app = *it;
        app->current_pid = get_target_pid(app);
        if (app->current_pid > 0) {
            pids.push_back(app->current_pid);
        }
    }
    return pids;
}

// 用于手动调试时打印内部数据
__attribute__((used)) void debug_print_inner_data()
{
    debug_print_configs();
    debug_print_records();
    DEBUG("---------------------------------------------------------------");
}

__attribute__((used)) void debug_dump_app_profile()
{
    for (auto it = configs->apps.begin(); it != configs->apps.end(); ++it) {
        AppConfig *app = *it;
        dump_app_profile_to_file(app);
    }
}
