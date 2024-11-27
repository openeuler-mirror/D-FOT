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
#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DataBuf {
    int len;
    void *data;
};

struct DataRingBuf {
    /* instance name */
    const char *instance_name;
    /* buf write index, initial value is -1 */
    int index;
    /* instance run times */
    uint64_t count;
    struct DataBuf *buf;
    int buf_len;
};

struct Param {
    const struct DataRingBuf **ring_bufs;
    int len;
};

struct Interface {
    const char* (*get_version)();
    /* The instance name is a unique identifier in the system. */
    const char* (*get_name)();
    const char* (*get_description)();
    /* Specifies the instance dependencies, which is used as the input information
     * for instance execution.
     */
    const char* (*get_dep)();
    /* Instance scheduling priority. In a uniform time period, a instance with a 
     * lower priority is scheduled first.
     */
    int (*get_priority)();
    int (*get_type)();
    /* Instance execution period. */
    int (*get_period)();
    bool (*enable)();
    void (*disable)();
    const struct DataRingBuf* (*get_ring_buf)();
    void (*run)(const struct Param*);
};

/* Obtains the instances from the plugin.
 * The return value is the number of instances.
 */
int get_instance(struct Interface **interface);

#ifdef __cplusplus
}
#endif

#endif
