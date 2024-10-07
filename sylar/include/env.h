/**
 * @file env.h
 * @brief 环境变量
 * @author beanljun
 * @date 2024-04-26
 */

#ifndef __ENV_H__
#define __ENV_H__

#include <map>
#include <string>
#include <vector>

#include "../util/singleton.h"
#include "mutex.h"

namespace sylar {

/**
 * @brief 环境变量类
 */
class Env {
public:
    typedef RWMutex RWMutexType;
    /**
     * @brief 初始化，包括记录程序名称与路径，解析命令行选项和参数
     * @details
     * 命令行选项全部以-开头，后面跟可选参数，选项与参数构成key-value结构，被存储到程序的自定义环境变量中，
     * 如果只有key没有value，那么value为空字符串
     * @param[in] argc main函数传入
     * @param[in] argv main函数传入
     * @return  是否成功
     */
    bool init(int argc, char** argv);
    /// 添加自定义环境变量，key-value结构，存储到map中
    void add(const std::string& key, const std::string& val);
    /// 获取是否存在键值为key的自定义环境变量
    bool has(const std::string& key);
    /// 删除键值为key的自定义环境变量
    void del(const std::string& key);
    /// 获取键值为key的自定义环境变量，如果未找到则返回默认值
    std::string get(const std::string& key, const std::string& default_value = "");


    /**
     * @brief 增加命令行帮助选项
     * @param[in] key 选项名
     * @param[in] desc 选项描述
     */
    void addHelp(const std::string& key, const std::string& desc);
    /// 删除命令行帮助选项
    void removeHelp(const std::string& key);
    /// 打印帮助信息
    void printHelp();
    /// 获取执行程序完整路径
    const std::string& getExe() const {
        return m_exe;
    }
    /// @brief 获取当前路径，从main函数的argv[0]中获取，以/结尾
    const std::string& getCwd() const {
        return m_cwd;
    }


    /// @brief 设置系统环境变量，参考setenv(3)
    bool setEnv(const std::string& key, const std::string& val);
    /// @brief 获取系统环境变量
    std::string getEnv(const std::string& key, const std::string& default_value = "");
    /// @brief 获取path的绝对路径
    /// @note 如果提供的path以/开头，那直接返回path即可，否则返回getCwd()+path
    std::string getAbsolutePath(const std::string& path) const;
    /// @brief 获取工作路径下path的绝对路径
    std::string getAbsoluteWorkPath(const std::string& path) const;
    /// @brief
    /// 获取配置文件路径，配置文件路径通过命令行-c选项指定，默认为当前目录下的conf文件夹
    std::string getConfigPath();

private:
    RWMutexType                                      m_mutex;
    std::map<std::string, std::string>               m_args;   /// 存储程序的自定义环境变量
    std::vector<std::pair<std::string, std::string>> m_helps;  /// 存储帮助选项与描述

    std::string m_program;  /// 程序名，也就是argv[0]
    std::string m_exe;      /// 程序完整路径名
    std::string m_cwd;      /// 当前路径，从argv[0]中获取
};

typedef sylar::Singleton<Env> EnvMgr;

}  // namespace sylar

#endif  // __SYLAR_ENV_H__