/**
 * @file config.h
 * @brief 配置管理模块
 * @author beanljun
 * @date 2024-04-27
*/

#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <map>
#include <set>
#include <list>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <sstream>
#include <functional>
#include <yaml-cpp/yaml.h>
#include <boost/lexical_cast.hpp>

#include "include/log.h"
#include "include/util.h"
#include "include/mutex.h"

namespace sylar {

    /// 配置变量的基类
    /// 抽象类，仅提供接口，不能实例化，只能用于派生
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    /**
     * @brief 构造函数
     * @param[in] name 配置参数名称
     * @param[in] description 配置参数描述
    */
    ConfigVarBase(const std::string& name, const std::string& description = "")
        :m_name(name)
        ,m_description(description) {
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }
    /// @brief 析构函数，虚函数，为了让派生类重写，释放资源
    virtual ~ConfigVarBase() {}

    /// @brief 获取配置参数名称
    const std::string& getName() const { return m_name; }

    /// @brief 获取配置参数描述
    const std::string& getDescription() const { return m_description; }

    /// @brief 转成字符串，纯虚函数，子类必须实现
    virtual std::string toString() = 0;

    /// @brief 从字符串初始化值
    virtual bool fromString(const std::string& val) = 0;

    /// @brief 获取配置参数的类型名称
    virtual std::string getTypeName() const = 0;

protected:
    std::string m_name;         // 配置参数名称
    std::string m_description;  // 配置参数描述
};

/// 配置参数模板子类, F=FromType, T=ToType
template <class F, class T>
class LexicalCast {
public:
    /**
     * @brief 类型转换，重载()操作符
     * @param[in] v 源类型值
     * @return 返回v转换后的目标类型
     * @exception 当类型不可转换时抛出异常
     */
    T operator()(const F& v) {
        return boost::lexical_cast<T>(v);
    }
};

/// 配置参数模板子类, YAML String => std::vector<T>
template <class T>
class LexicalCast<std::string, std::vector<T>> {
public:
    std::vector<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/// 配置参数模板子类, std::vector<T> => YAML String
template <class T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence); // 序列节点
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/// 配置参数模板子类, YAML String => std::list<T>
template <class T>
class LexicalCast<std::string, std::list<T>> {
public:
    std::list<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::list<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/// 配置参数模板子类, std::list<T> => YAML String
template <class T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator()(const std::list<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence); // 序列节点
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/// 配置参数模板子类, YAML String => std::set<T>
template <class T>
class LexicalCast<std::string, std::set<T>> {
public:
    std::set<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::set<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/// 配置参数模板子类, std::set<T> => YAML String
template <class T>
class LexicalCast<std::set<T>, std::string> {
public:
    std::string operator()(const std::set<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence); 
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/// 配置参数模板子类, YAML String => std::unordered_set<T>
template <class T>
class LexicalCast<std::string, std::unordered_set<T>> {
public:
    std::unordered_set<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_set<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/// 配置参数模板子类, std::unordered_set<T> => YAML String
template <class T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator()(const std::unordered_set<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence); 
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/// 配置参数模板子类, YAML String => std::map<std::string, T>
template <class T>
class LexicalCast<std::string, std::map<std::string, T>> {
public:
    std::map<std::string, T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::map<std::string, T> vec;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it -> second;
            vec.insert(std::make_pair(it -> first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

/// 配置参数模板子类, std::map<std::string, T> => YAML String
template <class T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
    std::string operator()(const std::map<std::string, T>& v) {
        YAML::Node node(YAML::NodeType::Map); 
        for(auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/// 配置参数模板子类, YAML String => std::unordered_map<std::string, T>
template <class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
public:
    std::unordered_map<std::string, T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_map<std::string, T> vec;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it -> second;
            vec.insert(std::make_pair(it -> first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

/// 配置参数模板子类, std::unordered_map<std::string, T> => YAML String
template <class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
public:
    std::string operator()(const std::unordered_map<std::string, T>& v) {
        YAML::Node node(YAML::NodeType::Map); 
        for(auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 配置参数模板子类, 保存对应类型的配置参数
 * @details T 配置参数的具体类型
 *          FromStr 从std::string转换为T的转换模板
 *          ToStr   从T转换为std::string的转换模板
 *          std::string 为YAML格式的字符串
 * @note 该类是一个具体类，可以实例化
*/
template <class T, class FromStr = LexicalCast<std::string, T>, class ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    typedef RWMutex RWMutexType;
    typedef std::shared_ptr<ConfigVar> ptr;
    typedef std::function<void (const T& old_value, const T& new_value)> on_change_cb;

    /**
     * @brief 构造函数
     * @param[in] name 配置参数名称
     * @param[in] default_value 配置参数的默认值
     * @param[in] description 配置参数描述
    */
    ConfigVar(const std::string& name ,const T& default_value ,const std::string& description = "")
        :ConfigVarBase(name, description)
        ,m_val(default_value) { }

    /**
     * @brief 将配置参数值转换成YAML String
     * @details try catch异常处理，转换失败时打印日志
     *          try {} 里面的代码可能会抛出异常，如果抛出异常，会被catch捕获，然后执行catch里面的代码
     * @exception 当转换失败时抛出异常
    */
    std::string toString() override {
        try {
            //return boost::lexical_cast<std::string>(m_val);
            RWMutexType::ReadLock lock(m_mutex);
            return ToStr()(m_val);
        } catch (std::exception &e) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception "
                                              << e.what() << " convert: " << TypeToName<T>() << " to string"
                                              << " name=" << m_name;
        }
        return "";
    }

    /// @brief 从YAML String初始化配置参数值，转换失败时抛出异常打印日志
    bool fromString(const std::string& val) override {
        try {
            setValue(FromStr()(val));
        } catch (std::exception &e) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::fromString exception "
                                              << e.what() << " convert: string to " << TypeToName<T>()
                                              << " name=" << m_name
                                              << " - " << val;
        }
        return false;
    }

    /// @brief 获取配置参数的值
    const T getValue() {
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }

    /// @brief 设置配置参数的值，如果参数值发生变化，调用回调函数
    void setValue(const T& v) {
        {
            RWMutexType::ReadLock lock(m_mutex);
            if(v == m_val) return;
            for(auto& i : m_cbs) {
                i.second(m_val, v);
            }
        }
        RWMutexType::WriteLock lock(m_mutex);
        m_val = v;
    }

    /// @brief 获取配置参数的类型名称
    std::string getTypeName() const override { return TypeToName<T>(); }

    /// @brief 添加变更回调函数，返回该回调函数的唯一id，用于删除
    uint64_t addListener(on_change_cb cb) {
        static uint64_t s_fun_id = 0;
        RWMutexType::WriteLock lock(m_mutex);
        ++s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    }

    /// @brief 删除回调函数
    void delListener(uint64_t key) {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.erase(key);
    }

    /// @brief 获取回调函数， key为回调函数的唯一id
    on_change_cb getListener(uint64_t key) {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it -> second;
    }

    /// @brief 清空所有回调函数
    void clearListener() {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }

private:
    RWMutexType m_mutex;                    // 读写锁
    T m_val;                                // 配置参数的值
    std::map<uint64_t, on_change_cb> m_cbs; // 回调函数集合
};


class Config {
public:
    typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
    typedef RWMutex RWMutexType;

    /**
     * @brief 查找配置参数
     * @param[in] name 配置参数名称
     * @param[in] default_value 参数的默认值
     * @param[in] description 参数描述
     * @details 如果名为name的配置参数存在，返回该配置参数
     *          如果不存在，创建一个默认值为default_value的配置参数，并返回
     * @exception 当name中包含非法字符[^0-9a-z_.]时，抛出异常
    */
    template <class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name, const T& default_value, const std::string& description = "") {
        RWMutexType::WriteLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if(it != GetDatas().end()) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it -> second);
            if(tmp) {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
                return tmp;
            } else {
                // 参数名存在但是类型不匹配则返回nullptr
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists but type not "
                                                  << TypeToName<T>() << " real_type=" << it->second->getTypeName()
                                                  << " " << it->second->toString();
                return nullptr;
            }
        }

        // 如果name中包含非法字符，抛出异常
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetDatas()[name] = v;
        return v;
    }

    /**
     * @brief 查找配置参数
     * @param[in] name 配置参数名称
     * @return 返回配置参数名为name的配置参数
    */
    template <class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if(it == GetDatas().end()) return nullptr;
        return std::dynamic_pointer_cast<ConfigVar<T>>(it -> second);
    }

    /// @brief 使用YAML::Node初始化配置模块
    static void LoadFromYaml(const YAML::Node& root);

    /// @brief 加载path文件夹下的所有配置文件
    static void LoadFromConfDir(const std::string& path, bool force = false);

    /// @brief 查找配置参数，返回配置参数的基类
    static ConfigVarBase::ptr LookupBase(const std::string& name);

    /// @brief 遍历所有配置项，cb为callback，回调函数
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

private:
    /// @brief 返回所有的配置项
    static ConfigVarMap& GetDatas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }

    /// @brief 配置项的读写锁
    static RWMutexType& GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }
};

}

#endif