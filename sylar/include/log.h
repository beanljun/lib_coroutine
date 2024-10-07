/**
 * @file log.h
 * @author beanljun
 * @brief 日志模块
 * @date 2024-04-25
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <cstdarg>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../util/singleton.h"
#include "../util/util.h"
#include "mutex.h"

/// 获取root日志器
#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()

/// 获取指定名称的日志器

#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

/**
 * @brief 将日志级别level的日志写入到logger
 * @details
 * 构造一个LogEventWrap对象，包裹包含日志器和日志事件，在对象析构时调用日志器写日志事件
 * @todo 协程id未实现，暂时写0
 */
#define SYLAR_LOG_LEVEL(logger, level)                                                                            \
    if (logger->getLevel() >= level)                                                                              \
    sylar::LogEventWrap(logger,                                                                                   \
                        sylar::LogEvent::ptr(new sylar::LogEvent(logger->getName(),                               \
                                                                 level,                                           \
                                                                 __FILE__,                                        \
                                                                 __LINE__,                                        \
                                                                 sylar::GetElapsedMS() - logger->getCreateTime(), \
                                                                 sylar::GetThreadId(),                            \
                                                                 sylar::GetFiberId(),                             \
                                                                 time(0),                                         \
                                                                 sylar::GetThreadName())))                        \
        .getLogEvent()                                                                                            \
        ->getSS()

#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_ALERT(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ALERT)

#define SYLAR_LOG_CRIT(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::CRIT)

#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)

#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)

#define SYLAR_LOG_NOTICE(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::NOTICE)

#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)

/**
 * @brief 使用C printf方式将日志级别level的日志写入到logger
 * @details
 * 构造一个LogEventWrap对象，包裹包含日志器和日志事件，在对象析构时调用日志器写日志事件
 * @todo 协程id未实现，暂时写0
 */

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...)                                                              \
    if (logger->getLevel() >= level)                                                                              \
    sylar::LogEventWrap(logger,                                                                                   \
                        sylar::LogEvent::ptr(new sylar::LogEvent(logger->getName(),                               \
                                                                 level,                                           \
                                                                 __FILE__,                                        \
                                                                 __LINE__,                                        \
                                                                 sylar::GetElapsedMS() - logger->getCreateTime(), \
                                                                 sylar::GetThreadId(),                            \
                                                                 sylar::GetFiberId(),                             \
                                                                 time(0),                                         \
                                                                 sylar::GetThreadName())))                        \
        .getLogEvent()                                                                                            \
        ->printf(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_ALERT(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ALERT, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_CRIT(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::CRIT, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_NOTICE(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::NOTICE, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)

namespace sylar {
///日志级别
class LogLevel {
public:
    /// 日志级别枚举
    enum Level {
        FATAL = 0,   // 致命错误
        ALERT = 1,   // 警报，高优先级情况
        CRIT = 2,    // 严重错误
        ERROR = 3,   // 一般错误
        WARN = 4,    // 警告
        NOTICE = 5,  // 通知
        INFO = 6,    // 一般信息
        DEBUG = 7,   // 调试信息
        NOTSET = 8   // 未设置
    };

    /// 将日志级别转换为字符串
    static const char* ToString(LogLevel::Level level);
    /// 将字符串转换为日志级别
    static LogLevel::Level FromString(const std::string& str);
};

/// 日志事件
class LogEvent {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<LogEvent> ptr;
    /**
     * @brief 构造函数
     * @param[in] logger_name 日志器名称
     * @param[in] level 日志级别
     * @param[in] file 文件名
     * @param[in] line 行号
     * @param[in] elapse 从日志器创建开始到当前的累计运行毫秒
     * @param[in] thread_id 线程id
     * @param[in] fiber_id 协程id
     * @param[in] time 日志事件(秒)
     * @param[in] thread_name 线程名称
     */
    LogEvent(const std::string& logger_name,
             LogLevel::Level    level,
             const char*        file,
             int32_t            line,
             int64_t            elapse,
             uint32_t           thread_id,
             uint64_t           fiber_id,
             time_t             time,
             const std::string& thread_name);

    /// 获取日志级别
    LogLevel::Level getLevel() const {
        return m_level;
    }

    /// 获取日志内容
    std::string getContent() const {
        return m_ss.str();
    }

    /// 获取文件名
    std::string getFile() const {
        return m_file;
    }

    /// 获取行号
    int32_t getLine() const {
        return m_line;
    }

    /// 获取累计运行时间
    int64_t getElapse() const {
        return m_elapse;
    }

    /// 获取线程id
    uint32_t getThreadId() const {
        return m_threadId;
    }

    /// 获取协程id
    uint32_t getFiberId() const {
        return m_fiberId;
    }

    /// 获取时间
    time_t getTime() const {
        return m_time;
    }

    /// 获取线程名称
    const std::string& getThreadName() const {
        return m_threadName;
    }

    /// 获取内容流，流式写入日志
    std::stringstream& getSS() {
        return m_ss;
    }

    /// 获取日志器名称
    const std::string& getLoggerName() const {
        return m_loggerName;
    }

    /// C prinf风格写入日志
    void printf(const char* fmt, ...);

    /// C vprinf风格写入日志
    void vprintf(const char* fmt, va_list al);

private:
    LogLevel::Level   m_level;           // 日志级别
    std::stringstream m_ss;              // 日志内容，使用stringstream存储，便于流式写入日志
    const char*       m_file = nullptr;  // 文件名
    int32_t           m_line = 0;        // 行号
    int64_t           m_elapse = 0;      // 程序启动开始到现在的毫秒数
    uint32_t          m_threadId = 0;    // 线程id
    uint64_t          m_fiberId = 0;     // 协程id
    time_t            m_time;            // 时间戳
    std::string       m_threadName;      // 线程名称
    std::string       m_loggerName;      // 日志器名称
};

/// 日志格式化
class LogFormatter {

public:
    typedef std::shared_ptr<LogFormatter> ptr;

    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板
     * @details 模板参数说明：
     * - %m 消息
     * - %p 日志级别
     * - %c 日志器名称
     * - %d 日期时间，后面可跟一对括号指定时间格式，比如%d{%Y-%m-%d
     * %H:%M:%S}，这里的格式字符与C语言strftime一致
     * - %r 该日志器创建后的累计运行毫秒数
     * - %f 文件名
     * - %l 行号
     * - %t 线程id
     * - %F 协程id
     * - %N 线程名称
     * - % 百分号
     * - %T 制表符
     * - %n 换行
     *
     * 默认格式：%d{%Y-%m-%d %H:%M:%S}
     * [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n
     *
     * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \\t 线程id \\t 线程名称
     * \\t 协程id \\t [日志级别] \\t [日志器名称] \\t 文件名:行号 \\t 日志消息
     * 换行符
     */
    LogFormatter(const std::string& pattern =
                     "%d{%Y-%m-%d %H:%M:%S} "
                     "[%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");

    /// 初始化，解析格式模板，提取模板项
    void init();

    /// 模板解析是否出错
    bool isError() const {
        return m_error;
    }

    /// 格式化日志事件，返回格式化日志文本
    std::string format(LogEvent::ptr event);

    /**
     * @brief 格式化日志事件，将格式化日志文本写入流
     * @param[in, out] os 输出流
     * @param[in] event 日志事件
     */
    std::ostream& format(std::ostream& os, LogEvent::ptr event);

    /// 获取pattern
    std::string getPattern() const {
        return m_pattern;
    }

public:
    /// 日志内容项格式化， 虚基类， 用于派生出不同的格式化项
    class FormatItem {
    public:
        /// 智能指针类型定义
        typedef std::shared_ptr<FormatItem> ptr;

        /// 析构函数
        virtual ~FormatItem() {}

        /**
         * @brief 格式化日志内容项
         * @param[out] os 输出流
         * @param[in] event 日志事件
         */
        virtual void format(std::ostream& os, LogEvent::ptr event) = 0;
    };

private:
    /// 格式模板
    std::string m_pattern;
    /// 格式解析后的格式项
    std::vector<FormatItem::ptr> m_items;
    /// 是否解析出错
    bool m_error = false;
};

/// 日志输出目标， 抽象类，用于派生出不同的LogAppender
///  参考log4cpp，Appender自带一个默认的LogFormatter，以控件默认输出格式
class LogAppender {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<LogAppender> ptr;
    typedef Spinlock                     MutexType;

    /// 构造函数
    LogAppender(LogFormatter::ptr default_formatter);

    /// 析构函数
    virtual ~LogAppender() {}

    /// 设置日志格式化器
    void setFormatter(LogFormatter::ptr val);

    /// 获取日志格式化器
    LogFormatter::ptr getFormatter();

    /// 写入日志事件, = 0 表示纯虚函数，子类必须实现
    virtual void log(LogEvent::ptr event) = 0;

    /// @brief 将日志输出目标的配置转成YAML String
    virtual std::string toYamlString() = 0;

protected:
    MutexType m_mutex;
    /// 日志格式化器
    LogFormatter::ptr m_formatter;
    /// 默认格式化器
    LogFormatter::ptr m_defaultFormatter;
};

/// 输出到控制台的Appender， 派生自LogAppender
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

    /// 构造函数
    StdoutLogAppender();

    /// 写入日志事件
    void log(LogEvent::ptr event) override;

    /// 将日志输出目标的配置转成YAML String
    std::string toYamlString() override;
};

/// 输出到文件的Appender， 派生自LogAppender
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    /**
     * @brief 构造函数，初始化文件路径，并打开
     * @param[in] filename 文件路径
     */
    FileLogAppender(const std::string& file);

    /// 写入日志事件
    void log(LogEvent::ptr event) override;

    /// 重新打开日志文件, 如果文件已经打开，先关闭文件，再打开文件
    bool reopen();

    /// 将日志输出目标的配置转成YAML String
    std::string toYamlString() override;

private:
    /// 文件路径
    std::string m_filename;
    /// 文件流
    std::ofstream m_filestream;
    /// 上次重新打开时间
    uint64_t m_lastTime = 0;
    /// 文件打开错误标志
    bool m_reopenError = false;
};

/// 日志器类
class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Spinlock                MutexType;

    /// 构造函数
    Logger(const std::string& name = "defalut");

    /// 获取日志器名称
    const std::string& getName() const {
        return m_name;
    }

    /// 获取日志创建时间
    const uint64_t& getCreateTime() const {
        return m_createTime;
    }

    /// 设置日志级别
    void setLevel(LogLevel::Level level) {
        m_level = level;
    }

    /// 获取日志级别
    LogLevel::Level getLevel() const {
        return m_level;
    }

    /// 添加Appender
    void addAppender(LogAppender::ptr appender);

    /// 删除Appender
    void delAppender(LogAppender::ptr appender);

    /// 清空Appender
    void clearAppenders();

    /**
     * 调用Logger的所有appenders将日志写一遍，
     * Logger至少要有一个appender，否则没有输出
     */
    void log(LogEvent::ptr event);

    /// 将日志器的配置转成YAML String
    std::string toYamlString();

private:
    /// 互斥锁
    MutexType m_mutex;
    /// 日志器名称
    std::string m_name;
    /// 日志级别
    LogLevel::Level m_level;
    /// 日志LogAppender集合
    std::list<LogAppender::ptr> m_appenders;
    /// 创建时间
    uint64_t m_createTime;
};


/// 日志事件包裹器，方便宏定义，内部包含日志事件和日志器
class LogEventWrap {
public:
    /// 构造函数
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event);

    /// 析构函数
    ~LogEventWrap();

    /// 获取日志事件
    LogEvent::ptr getLogEvent() const {
        return m_event;
    }

private:
    Logger::ptr   m_logger;  // 日志器
    LogEvent::ptr m_event;   // 日志事件
};

/// 日志器管理类
class LoggerManager {
public:
    typedef Spinlock MutexType;

    /// 构造函数
    LoggerManager();

    /// 获取日志器
    Logger::ptr getLogger(const std::string& name);

    /// 初始化，结合配置模块实现日志模块初始化
    void init();

    /// 获取root日志器 ==> getLogger("root")
    Logger::ptr getRoot() const {
        return m_root;
    }

    /// 将所有的日志配置转成YAML String
    std::string toYamlString();

private:
    MutexType m_mutex;
    /// 日志器集合
    std::map<std::string, Logger::ptr> m_loggers;
    /// root日志器
    Logger::ptr m_root;
};

/// 日志器管理类单例
typedef sylar::Singleton<LoggerManager> LoggerMgr;

}  // namespace sylar

#endif