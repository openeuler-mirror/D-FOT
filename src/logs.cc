#include "logs.h"

Logger::Logger(std::string name)
{
    logger = log4cplus::Logger::getInstance(name);
    if (logger.getLogLevel() != log4cplus::NOT_SET_LOG_LEVEL) {
        // 已经实例化，无需处理
        return;
    }
    log4cplus::SharedAppenderPtr appender(new log4cplus::ConsoleAppender()); 
    appender->setName("console");
    log4cplus::tstring pattern =
        LOG4CPLUS_TEXT("%D{%m/%d/%y %H:%M:%S} [%t] %-5p %c - %m%n");
    appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
    logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
    logger.addAppender(appender);
}

void Logger::init()
{   
    if (logger.getAppender("file") != nullptr) {
        return;
    }

    log4cplus::SharedAppenderPtr appender(new log4cplus::FileAppender(DFOT_LOG_PATH, std::ios_base::app));
    appender->setName("file");
    log4cplus::tstring pattern =
        LOG4CPLUS_TEXT("%D{%m/%d/%y %H:%M:%S} [%t] %-5p %c - %m%n");
    appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
    logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
    logger.addAppender(appender);
}
