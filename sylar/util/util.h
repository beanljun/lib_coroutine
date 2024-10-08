/**
 * @file util.h
 * @author beanljun
 * @brief
 * @date 2024-04-25
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <cxxabi.h>  // for abi::__cxa_demangle()
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include <iostream>
#include <string>
#include <vector>

namespace sylar {

/**
 * @brief 获取当前线程ID
 * @note 注意pid_t和pthread_t的区别
 */
pid_t GetThreadId();

/**
 * @brief 获取当前协程ID
 * @todo 暂未实现，返回0
 */
uint64_t GetFiberId();

/**
 * @brief 获取当前启动的毫秒，使用clock_gettime()、CLOCK_MONOTONIC_RAW
 */
uint64_t GetElapsedMS();

/**
 * @brief 获取线程的名称，利用pthread_getname_np()
 */
std::string GetThreadName();

/**
 * @brief 设置线程的名称，不超过15个字符，包括'\0'，利用pthread_setname_np()
 */
void SetThreadName(const std::string& name);

/**
 * @brief 获取当前调用栈
 * @param[out] bt 保存调用栈
 * @param[in] size 最多返回层数
 * @param[in] skip 跳过栈顶的层数
 */
void Backtrace(std::vector<std::string>& bt, int size, int skip = 1);

/**
 * @brief 获取当前栈信息的字符串
 * @param[in] size 栈的最大层数
 * @param[in] skip 跳过栈顶的层数
 * @param[in] prefix 栈信息前输出的内容
 */
std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");

/// @brief 获取当前时间的毫秒， gettimeofday()
uint64_t GetCurrentMS();

/// @brief 获取当前时间的微秒， gettimeofday()
uint64_t GetCurrentUS();

/// @brief 字符串转大写， std::transform() + ::toupper()
std::string ToUpper(const std::string& name);

/// @brief 字符串转小写， std::transform() + ::tolower()
std::string ToLower(const std::string& name);

/// @brief 时间戳转字符串， localtime_r() + strftime()
std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");

/// @brief 字符串转时间戳， strptime()
time_t Str2Time(const char* str, const char* format = "%Y-%m-%d %H:%M:%S");

/// @brief 文件系统操作类
class FSUtil {
public:
    /**
     * @brief 读取文件内容
     * @details
     * 递归列举指定目录下所有指定后缀的常规文件，如果不指定后缀，则遍历所有文件，返回的文件名带路径
     * @param[out] files 文件列表
     * @param[in] path 路径
     * @param[in] subfix 后缀名，比如 ".yml"
     */
    static void ListAllFile(std::vector<std::string>& files, const std::string& path, const std::string& subfix);
    /// @brief 创建目录
    /// @param[in] dirname 目录名
    static bool Mkdir(const std::string& dirname);

    /// @brief 判断pid文件指定的pid号是否在运行，kill(pid, 0)
    /// @param[in] pidfile 保存进程号的文件
    static bool IsRunningPidfile(const std::string& pidfile);

    /// @brief 删除文件或目录
    static bool Rm(const std::string& path);

    /// @brief 移动文件或路径
    static bool Mv(const std::string& from, const std::string& to);

    /// @brief 返回绝对路径， realpath()
    /// @details 路径中的符号链接会被解析成实际的路径，删除多余的'.' '..'和'/'
    static bool Realpath(const std::string& path, std::string& rpath);

    /// @brief 创建符号链接
    static bool Symlink(const std::string& from, const std::string& to);

    /// @brief 删除符号链接
    static bool Unlink(const std::string& filename, bool exist = false);

    /// @brief 返回文件的目录名
    static std::string Dirname(const std::string& filename);

    /// @brief 返回文件名
    static std::string Basename(const std::string& filename);

    /// @brief 以只读方式打开
    static bool OpenForRead(std::ifstream& ifs, const std::string& filename, std::ios_base::openmode mode);

    /// @brief 以只写方式打开
    static bool OpenForWrite(std::ofstream& ofs, const std::string& filename, std::ios_base::openmode mode);
};

/// @brief 类型转换类
class TypeUtil {
public:
    /// 转为字符，返回*str.begin()
    static int8_t ToChar(const char* str);
    static int8_t ToChar(const std::string& str);

    /// atoi()
    static int64_t Atoi(const char* str);
    static int64_t Atoi(const std::string& str);

    /// atof()
    static double Atof(const char* str);
    static double Atof(const std::string& str);
};

/// @brief 获取T类型的类型字符串
template <class T>
const char* TypeToName() {
    static const char* s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
    return s_name;
}

/// @brief 字符串操作类
class StringUtil {
public:
    /// printf风格的字符串格式化，返回格式化后的string
    static std::string Format(const char* fmt, ...);

    /// vprintf风格的字符串格式化，返回格式化后的string
    static std::string Formatv(const char* fmt, va_list ap);

    /**
     * @brief url编码
     * @param[in] str 原始字符串
     * @param[in] space_as_plus
     * 是否将空格编码成+号，如果为false，则空格编码成%20
     * @return 编码后的字符串
     */
    static std::string UrlEncode(const std::string& str, bool space_as_plus = true);

    /**
     * @brief url解码
     * @param[in] str url字符串
     * @param[in] space_as_plus 是否将+号解码为空格
     * @return 解析后的字符串
     */
    static std::string UrlDecode(const std::string& str, bool space_as_plus = true);

    /**
     * @brief 移除字符串首尾的指定字符串
     * @param[] str 输入字符串
     * @param[] delimit 待移除的字符串
     * @return  移除后的字符串
     */
    static std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");

    /**
     * @brief 移除字符串首部的指定字符串
     * @param[] str 输入字符串
     * @param[] delimit 待移除的字符串
     * @return  移除后的字符串
     */
    static std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");

    /**
     * @brief 移除字符尾部的指定字符串
     * @param[] str 输入字符串
     * @param[] delimit 待移除的字符串
     * @return  移除后的字符串
     */
    static std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");

    /// 宽字符串转字符串
    static std::string WStringToString(const std::wstring& ws);

    /// 字符串转宽字符串
    static std::wstring StringToWString(const std::string& s);
};

}  // namespace sylar

#endif