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
#ifndef __RECORDS_H__
#define __RECORDS_H__

#include "configs.h"

typedef struct {
    BinaryInstance *instance;
    pid_t pid;
    int64_t ts;
} Pidinfo;

typedef struct {
    uint64_t processed_samples;
    std::map<pid_t, Pidinfo*> pids;
    std::map<const char*, bool> modules;
} global_records;

extern global_records records;

extern void reset_records();
extern void debug_print_records();

#endif
