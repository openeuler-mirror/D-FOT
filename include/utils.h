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
#ifndef __UTILS_H__
#define __UTILS_H__

#include <vector>
#include <map>
#include <mutex>

#include "configs.h"

#define INVALID_PID -1

typedef struct {
    std::string cmd_log;
    int ret;
} exec_result;

extern bool get_real_path(const char* path, char* resolved);
extern exec_result exec_cmd(std::string cmd);
extern time_t get_file_create_time(std::string file_path);
extern std::string turn_timestamp_to_format_time(int64_t timestamp);
extern int64_t get_current_timestamp();
extern std::string get_bin_full_path_by_pid(pid_t pid);

#endif
