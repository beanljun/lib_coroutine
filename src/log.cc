/**
 * @file log.cc
 * @author beanljun
 * @brief 日志模块实现
 * @date 2024-04-25
*/

#include <utility>
#include "include/log.h"

namespace sylar {

const char* LogLevel::ToString(LogLevel::Level level) {
        switch (level) {
#define XX(name) case LogLevel::name: return #name; 
        XX(FATAL);
        XX(ALERT);
        XX(CRIT);
        XX(ERROR);
        XX(WARN);
        XX(NOTICE);
        XX(INFO);
        XX(DEBUG);
#undef XX
        default:
            return "NOTSET";
        }
        return "NOTSET";
    }

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, v) if (str == #v) { return LogLevel::level; }
    XX(FATAL, fatal);
    XX(ALERT, alert);
    XX(CRIT, crit);
    XX(ERROR, error);
    XX(WARN, warn);
    XX(NOTICE, notice);
    XX(INFO, info);
    XX(DEBUG, debug);

    XX(FATAL, FATAL);
    XX(ALERT, ALERT);
    XX(CRIT, CRIT);
    XX(ERROR, ERROR);
    XX(WARN, WARN);
    XX(NOTICE, NOTICE);
    XX(INFO, INFO);
    XX(DEBUG, DEBUG);
#undef XX
    return LogLevel::NOTSET;
}

LogEvent::LogEvent(const std::string& logger_name, LogLevel::Level level, const char* file, int32_t line, 
                    int64_t elapse, uint32_t thread_id, uint64_t fiber_id, time_t time, 
                    const std::string& thread_name)
    : m_level(level)
    , m_file(file)
    , m_line(line)
    , m_elapse(elapse)
    , m_threadId(thread_id)
    , m_fiberId(fiber_id)
    , m_time(time)
    , m_threadName(thread_name)
    , m_loggerName(logger_name) { }

    void LogEvent::printf(const char* fmt, ...) {
        va_list al;
        va_start(al, fmt);
        vprintf(fmt, al);
        va_end(al);
    }

    void LogEvent::vprintf(const char* fmt, va_list al) {
        char* buf = nullptr;
        int len = vasprintf(&buf, fmt, al);
        if (len != -1) {
            m_ss << std::string(buf, len);
            free(buf);
        }
    }

    /**
     * 以下分别实现了日志格式器的各个格式化项，
     * 他们都继承自LogFormatter::FormatItem，实现了format方法！！！
     * 1. MessageFormatItem: 日志内容格式化项
     * 2. LevelFormatItem: 日志级别格式化项
     * 3. ElapseFormatItem: 日志耗时格式化项
     * 4. LoggerNameFormatItem: 日志名称格式化项
     * 5. ThreadIdFormatItem: 线程id格式化项
     * 6. FiberIdFormatItem: 协程id格式化项
     * 7. ThreadNameFormatItem: 线程名称格式化项
     * 8. DateTimeFormatItem: 日期时间格式化项
     * 9. FilenameFormatItem: 文件名格式化项
     * 10. LineFormatItem: 行号格式化项
     * 11. NewLineFormatItem: 换行格式化项
     * 12. StringFormatItem: 字符串格式化项
     * 13. TabFormatItem: Tab格式化项
     * 14. PercentFormatItem: %格式化项
    */

    /// 日志格式器
    class MessageFormatItem : public LogFormatter::FormatItem {
    public:
        MessageFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getContent(); }
    };

    /// 日志级别格式器
    class LevelFormatItem : public LogFormatter::FormatItem {
    public:
        LevelFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << LogLevel::ToString(event -> getLevel()); }
    };

    /// 日志时间格式器
    class ElapseFormatItem : public LogFormatter::FormatItem {
    public:
        ElapseFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getElapse(); }
    };

    /// 日志名称格式器
    class LoggerNameFormatItem : public LogFormatter::FormatItem {
    public:
        LoggerNameFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getLoggerName(); }
    };

    /// 线程id格式器
    class ThreadIdFormatItem : public LogFormatter::FormatItem {
    public:
        ThreadIdFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getThreadId(); }
    };

    /// 协程id格式器
    class FiberIdFormatItem : public LogFormatter::FormatItem {
    public:
        FiberIdFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getFiberId(); }
    };

    /// 线程名称格式器
    class ThreadNameFormatItem : public LogFormatter::FormatItem {
    public:
        ThreadNameFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getThreadName(); }
    };

    /// 日期时间格式器
    class DateTimeFormatItem : public LogFormatter::FormatItem {
    public:
        DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S") 
            : m_format(format) {
            if(m_format.empty())    m_format = "%Y-%m-%d %H:%M:%S"; 
        }

        void format(std::ostream& os, LogEvent::ptr event) override {
            struct tm tm;
            time_t time = event -> getTime();                   
            localtime_r(&time, &tm);                            // 将时间转换为tm结构体
            char buf[64];
            strftime(buf, sizeof(buf), m_format.c_str(), &tm);  // 格式化时间
            os << buf;
        }
    private:
        std::string m_format;
    };

    /// 文件名格式器
    class FileNameFormatItem : public LogFormatter::FormatItem {
    public:
        FileNameFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getFile(); }
    };

    /// 行号格式器
    class LineFormatItem : public LogFormatter::FormatItem {
    public:
        LineFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << event -> getLine(); }
    };

    /// 换行格式器
    class NewLineFormatItem : public LogFormatter::FormatItem {
    public:
        NewLineFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << std::endl; }
    };

    /// 字符串格式器
    class StringFormatItem : public LogFormatter::FormatItem {
    public:
        StringFormatItem(const std::string& str) : m_string(str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << m_string; }
    private:
        std::string m_string;
    };

    /// Tab格式器
    class TabFormatItem : public LogFormatter::FormatItem {
    public:
        TabFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << "\t"; }
    };

    /// % 格式器
    class PercentFormatItem : public LogFormatter::FormatItem {
    public:
        PercentFormatItem(const std::string& str) {}
        void format(std::ostream& os, LogEvent::ptr event) override { os << "%"; }
    };

    LogFormatter::LogFormatter(const std::string& pattern) : m_pattern(pattern) {
        init();
    }


    void LogFormatter::init() {
        /// 按顺序存放解析出来的pattern项
        /// 每个pattern项是一个pair，第一个元素是pattern的类型，第二个元素是pattern的内容
        /// 类型为0表示普通字符串，类型为1表示需要格式化的项
        std::vector<std::pair<int, std::string>> patterns;
        std::string tmp;            /// 临时存储常规字符串
        std::string dateformat;     /// 日期格式字符串，默认把位于%d后面的大括号对里的全部字符都当作格式字符，不校验格式是否合法
        bool error = false;         /// 标记解析是否出现错误
        bool parsing_string = true; /// 标记当前是否在解析普通字符串, 初始为true

        size_t i = 0;               /// 当前解析到的位置
        while(i < m_pattern.size()) {
            std::string c = std::string(1, m_pattern[i]);
            if(c == "%") {
                if(parsing_string) {
                    if(!tmp.empty())  patterns.push_back(std::make_pair(0, tmp));
                    tmp.clear();
                    parsing_string = false; /// 在解析常规字符时遇到%，表示开始解析模板字符
                    i++;
                    continue;
                } else {
                    patterns.push_back(std::make_pair(1, c));
                    parsing_string = true;
                    i++;
                    continue;
                }   
            } else {    /// 当前字符不是%
                if(parsing_string) { /// 在解析常规字符, 直接加入tmp
                    tmp += c;
                    i++;
                    continue;
                } else {  // 模板字符，直接添加到patterns中，添加完成后，状态变为解析常规字符，%d特殊处理
                    patterns.push_back(std::make_pair(1, c));
                    parsing_string = true;

                    // 如果%d后面直接跟了一对大括号，那么把大括号里面的内容提取出来作为dateformat
                    if(c == "d") {  
                        i++;
                        continue;
                    }
                    i++;
                    if(i < m_pattern.size() && m_pattern[i] == '{')  continue;
                    i++;
                    while(i < m_pattern.size() && m_pattern[i] != '}') {
                        dateformat.push_back(m_pattern[i]);
                        i++;
                    }
                    if(m_pattern[i] != '}') {
                        // %d后面的大括号没有闭合，直接报错
                        std::cout << "[ERROR] LogFormatter::init() " << "pattern: [" << m_pattern << "] '{' not closed" << std::endl;
                        error = true;   // 标记解析出错
                        break;
                    }
                    i++;
                    continue;
                }
            }
        }

        if(error) {
            m_error = true;
            return;
        }

        // 模板解析结束之后剩余的常规字符也要算进去
        if(!tmp.empty()) {
            patterns.push_back(std::make_pair(0, tmp));
            tmp.clear();
        }

        static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C)  {#str, [](const std::string& fmt) { return FormatItem::ptr(new C(fmt));} }
            XX(m, MessageFormatItem),           // m:消息
            XX(p, LevelFormatItem),             // p:日志级别
            XX(c, LoggerNameFormatItem),        // c:日志器名称
            XX(r, ElapseFormatItem),            // r:累计毫秒数
            XX(f, FileNameFormatItem),          // f:文件名
            XX(l, LineFormatItem),              // l:行号
            XX(t, ThreadIdFormatItem),          // t:编程号
            XX(F, FiberIdFormatItem),           // F:协程号
            XX(N, ThreadNameFormatItem),        // N:线程名称
            XX(%, PercentFormatItem),           // %:百分号
            XX(T, TabFormatItem),               // T:制表符
            XX(n, NewLineFormatItem),           // n:换行符
#undef XX
        };

        for(auto& v : patterns) {
            if( v.first == 0) m_items.push_back(FormatItem::ptr(new StringFormatItem(v.second)));
            else if(v.second == "d") m_items.push_back(FormatItem::ptr(new DateTimeFormatItem(dateformat)));
            else {
                auto it = s_format_items.find(v.second);
                if(it == s_format_items.end()) {
                    std::cout << "[ERROR] LogFormatter::init() " << "pattern: [" << m_pattern << "] " << 
                    "unknown format item: " << v.second << std::endl;
                    error = true;
                    break;
                } else m_items.push_back(it -> second(v.second));
            }
        }

        if(error) {
            m_error = true;
            return;
        }

}

///  格式化日志事件，返回格式化后的字符串
std::string LogFormatter::format(LogEvent::ptr event) {
    std::stringstream ss;
    for(auto& i : m_items) i -> format(ss, event);
    return ss.str();
}

/// 格式化日志事件，将格式化后的字符串写入到流中
std::ostream& LogFormatter::format(std::ostream& os, LogEvent::ptr event) {
    for(auto& i : m_items) i -> format(os, event);
    return os;
}

/// 日志输出地
LogAppender::LogAppender(LogFormatter::ptr default_formatter)  : m_defaultFormatter(default_formatter) {}

/// 写入日志事件
void LogAppender::setFormatter(LogFormatter::ptr val) {
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
}

/// 获取日志格式器
LogFormatter::ptr LogAppender::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

/// 输出控制台日志
StdoutLogAppender::StdoutLogAppender() : LogAppender(LogFormatter::ptr(new LogFormatter)) { }

void StdoutLogAppender::log(LogEvent::ptr event) {
    if(m_formatter) m_formatter -> format(std::cout, event); 
    else     m_defaultFormatter -> format(std::cout, event);
}
/// 输出文件日志
FileLogAppender::FileLogAppender(const std::string& file) : LogAppender(LogFormatter::ptr(new LogFormatter)) {
    m_filename = file;
    reopen();
}

/// 距离上次写日志超过3秒，重新打开一次日志文件
void FileLogAppender::log(LogEvent::ptr event) {
    time_t now = event -> getTime();
    // 如果当前时间距离上次写日志超过3秒，那就重新打开一次日志文件
    if(now >= m_lastTime + 3) {
        reopen();
        m_lastTime = now;
    }

    MutexType::Lock lock(m_mutex);
    if(m_formatter) {
        // 格式化器存在，用格式化器格式化日志事件，如果格式化失败，输出错误信息
        if(!m_formatter -> format(m_filestream, event)) std::cout << "[ERROR] FileLogAppender::log() " << "format error" << std::endl;
    } else {
        // 格式化器不存在，用默认格式化器格式化日志事件，如果格式化失败，输出错误信息
        if(!m_defaultFormatter -> format(m_filestream, event)) std::cout << "[ERROR] FileLogAppender::log() " << "format error" << std::endl;
    }
}

/// 重新打开日志文件, 如果文件已经打开，先关闭文件，再打开文件
bool FileLogAppender::reopen() {
    MutexType::Lock lock(m_mutex);
    if(m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename, std::ios::app);
    return !!m_filestream;
}

/// 日志器构造函数
Logger::Logger(const std::string& name) 
    : m_name(name)
    , m_level(LogLevel::INFO)
    , m_createTime(GetElapsed()) { }


void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    for(auto it = m_appenders.begin(); it != m_appenders.end(); it++) {
        if(*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::clearAppenders() {
    MutexType::Lock lock(m_mutex);
    m_appenders.clear();
}

/**
 * 调用Logger的所有appenders将日志写一遍，
 * Logger至少要有一个appender，否则没有输出
 */

void Logger::log(LogEvent::ptr event) {
    if(m_level >= event -> getLevel()) {
        for(auto& i : m_appenders) i -> log(event);
    }
}

LogEventWrap::LogEventWrap(Logger::ptr Logger, LogEvent::ptr event) : m_logger(Logger), m_event(event) { }

// LogEventWrap在析构时写日志，在销毁时将m_event记录到日志中

LogEventWrap::~LogEventWrap() {
    m_logger -> log(m_event);
}

LoggerManager::LoggerManager() {
    m_root.reset(new Logger("root"));
    m_root -> addAppender(LogAppender::ptr(new StdoutLogAppender));
    m_loggers[m_root -> getName()] = m_root;
    init();
}
/**
 * 如果指定名称的日志器未找到，那会就新创建一个，但是新创建的Logger是不带Appender的，
 * 需要手动添加Appender
 */
Logger::ptr LoggerManager::getLogger(const std::string& name) {
    MutexType::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()) return it -> second;

    Logger::ptr logger(new Logger(name));
    m_loggers[name] = logger;
    return logger;
}
/// @todo 从配置文件中加载日志器
void LoggerManager::init() {}

} // namespace sylar