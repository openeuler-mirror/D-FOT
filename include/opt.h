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
#ifndef __OPT_H__
#define __OPT_H__

#include <string>

#include <libkperf/pmu.h>
#include "configs.h"

extern bool check_dependence_ready();
extern bool is_app_eligible_for_optimization(AppConfig *app);
extern std::string get_app_profile(AppConfig *app);
extern void process_pmudata(struct PmuData *data, int len);
extern void do_optimize(AppConfig *app, std::string profile);

#endif
