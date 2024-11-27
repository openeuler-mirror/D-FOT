#include <iostream>
#include <sys/stat.h>
#include <limits.h>
#include <iomanip>
#include <functional>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "utils.h"

// 获取二进制buildid信息，预留接口
std::string get_bin_build_id(std::string full_path)
{
    (void)full_path;
    return "Not implemented";
}

// 获取二进制hash值，预留接口
std::string get_exec_hash(std::string full_path)
{
    (void)full_path;
    return "Not implemented";
}

bool get_real_path(const char* path, char* resolved)
{
    if (realpath(path, resolved) == nullptr) {
        ERROR("Error resolving path: " << path << " - " << strerror(errno));
        return false;
    }
    return true;
}

exec_result exec_cmd(std::string cmd)
{
    std::string log;
    int ret;
    DEBUG("exec cmd: \"" << cmd << "\"");
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        ERROR("popen failed");
        return {"", -1};
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        log += buffer;
    }

    ret = pclose(pipe);
    return {log, ret};
}

// 获取文件的创建时间（秒时间戳）
time_t get_file_create_time(std::string file_path)
{
    struct stat st;
    if (stat(file_path.c_str(), &st) == -1) {
        return -1;
    }
    return st.st_ctime;
}

// 将毫秒时间戳转换成格式化时间字符串
std::string turn_timestamp_to_format_time(int64_t timestamp)
{
    time_t time = static_cast<time_t>(timestamp/1000);
    char buffer[20] = {0};
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&time));
    return std::string(buffer);
}

// 获取当前毫秒时间戳
int64_t get_current_timestamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// 根据pid获取实际二进制的绝对路径
std::string get_bin_full_path_by_pid(pid_t pid)
{
    std::string path = "/proc/" + std::to_string(pid) + "/exe";
    std::vector<char> buffer(1024);

    // 解析符号链接，获取实际路径
    ssize_t len = readlink(path.c_str(), buffer.data(), buffer.size() - 1);
    if (len == -1) {
        WARN("readlink failed: " << strerror(errno));
        return "";
    }
    
    buffer[len] = '\0';
    return std::string(buffer.data());
}