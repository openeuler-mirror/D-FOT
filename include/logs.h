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
#ifndef __LOGS_H__
#define __LOGS_H__

#include <string>
#include <map>

#include <log4cplus/log4cplus.h>

#define DFOT_OK     0
#define DFOT_ERROR -1

#define DFOT_LOG_PATH "/etc/dfot/dfot.log"

#define DEBUG(fmt) LOG4CPLUS_DEBUG(logger.get(), fmt)
#define INFO(fmt)  LOG4CPLUS_INFO(logger.get(), fmt)
#define WARN(fmt)  LOG4CPLUS_WARN(logger.get(), fmt)
#define ERROR(fmt) LOG4CPLUS_ERROR(logger.get(), fmt)
#define FATAL(fmt) LOG4CPLUS_FATAL(logger.get(), fmt)

static std::map<std::string, log4cplus::LogLevel> LOG_LEVEL = {
    {"DEBUG", log4cplus::DEBUG_LOG_LEVEL},
    {"INFO", log4cplus::INFO_LOG_LEVEL},
    {"WARN", log4cplus::WARN_LOG_LEVEL},
    {"ERROR", log4cplus::ERROR_LOG_LEVEL},
    {"FATAL", log4cplus::FATAL_LOG_LEVEL},
};

class Logger {
public:
    Logger(std::string name);
    void init();
    log4cplus::Logger get() {
        return logger;
    }
    void setLogLevel(log4cplus::LogLevel log_level) {
        logger.setLogLevel(log_level);
    }
private:
    log4cplus::Logger logger;
    log4cplus::Initializer initializer;
};

extern Logger logger;

#endif
