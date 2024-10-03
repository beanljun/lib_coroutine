/**
 * @file uri.h
 * @brief URI封装类
 * @author beanljun
 * @date 2024-10-04
 */
#ifndef __SYLAR_URI_H__
#define __SYLAR_URI_H__

#include <memory>
#include <string>
#include <stdint.h>
#include "address.h"

namespace sylar {

/*
     foo://user@sylar.com:8042/over/there?name=ferret#nose
       \_/   \______________/\_________/ \_________/ \__/
        |           |            |            |        |
     scheme     authority       path        query   fragment
*/

class Uri {
public:
    typedef std::shared_ptr<Uri> ptr;

    /**
     * @brief 创建Uri对象
     * @param uri uri字符串
     * @return 解析成功返回Uri对象否则返回nullptr
     */
    static Uri::ptr Create(const std::string& uri);

    Uri();

    /// 获取scheme
    const std::string& getScheme() const { return m_scheme;}

    /// 获取用户信息
    const std::string& getUserinfo() const { return m_userinfo;}

    /// 获取host
    const std::string& getHost() const { return m_host;}

    /// 获取路径
    const std::string& getPath() const;

    /// 获取查询条件
    const std::string& getQuery() const { return m_query;}

    /// 获取fragment
    const std::string& getFragment() const { return m_fragment;}

    /// 获取端口
    int32_t getPort() const;

    /// 设置scheme
    void setScheme(const std::string& v) { m_scheme = v;}

    /// 设置用户信息
    void setUserinfo(const std::string& v) { m_userinfo = v;}

    /// 设置host
    void setHost(const std::string& v) { m_host = v;}

    /// 设置路径
    void setPath(const std::string& v) { m_path = v;}

    /// 设置查询条件
    void setQuery(const std::string& v) { m_query = v;}

    /// 设置fragment
    void setFragment(const std::string& v) { m_fragment = v;}

    /// 设置端口
    void setPort(int32_t v) { m_port = v;}

    /// 序列化到输出流
    std::ostream& dump(std::ostream& os) const;

    /// 转成字符串
    std::string toString() const;

    /// 获取Address
    Address::ptr createAddress() const;
private:

    /// 是否默认端口
    bool isDefaultPort() const;
private:
    /// schema
    std::string m_scheme;       // scheme
    std::string m_userinfo;     // 用户信息
    std::string m_host;         // host
    std::string m_path;         // 路径
    std::string m_query;        // 查询参数
    std::string m_fragment;     // fragment
    int32_t     m_port;         // 端口
};

}

#endif
