/**
 * @file address.h
 * @brief 地址管理模块
 * @author beanljun
 * @date 2024-09-23
 */

#ifndef __ADDRESS_H__
#define __ADDRESS_H__

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <map>
#include <memory>
#include <string>
#include <vector>


namespace sylar {
class IPAddress;

// 地址抽象类
class Address {
public:
    typedef std::shared_ptr<Address> ptr;

    /**
     * @brief 通过sockaddr指针创建Address
     * @param[in] addr sockaddr指针
     * @param[in] addrlen sockaddr的长度
     * @return 返回和sockaddr相匹配的Address,失败返回nullptr
     */
    static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);

    /**
     * @brief 通过域名解析IP地址,获取满足条件的所有Address
     * @param[in] result 解析结果，保存解析到的address
     * @param[in] host 域名
     * @param[in] family 地址族(AF_INET, AF_INET6, AF_UNIX)
     * @param[in] type 套接字类型(SOCK_STREAM, SOCK_DGRAM)
     * @param[in] protocol 协议(IPPROTO_TCP, IPPROTO_UDP)
     * @return 成功返回true,失败返回false
     */
    static bool Lookup(std::vector<Address::ptr>& result,
                       const std::string&         host,
                       int                        family = AF_INET,
                       int                        type = 0,
                       int                        protocol = 0);

    /**
     * @brief 获取任意一个Address
     * @param[in] host 域名
     * @param[in] family 地址族(AF_INET, AF_INET6, AF_UNIX)
     * @param[in] type 套接字类型(SOCK_STREAM, SOCK_DGRAM)
     * @param[in] protocol 协议(IPPROTO_TCP, IPPROTO_UDP)
     * @return 成功返回address,失败返回nullptr
     */
    static Address::ptr LookupAny(const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);

    /**
     * @brief 获取任意一个对应条件的IP地址
     * @param[in] host 域名
     * @param[in] family 地址族(AF_INET, AF_INET6, AF_UNIX)
     * @param[in] type 套接字类型(SOCK_STREAM, SOCK_DGRAM)
     * @param[in] protocol 协议(IPPROTO_TCP, IPPROTO_UDP)
     * @return 返回满足条件的任意IPAddress,失败返回nullptr
     */
    static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                                                         int                family = AF_INET,
                                                         int                type = 0,
                                                         int                protocol = 0);

    /**
     * @brief 返回本机所有网卡的<网卡名, 地址, 子网掩码位数>
     * @param[in] result 保存结果
     * @param[in] family 地址族(AF_INET, AF_INET6, AF_UNIX)
     * @return 成功返回true,失败返回false
     */
    static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
                                      int                                                            family = AF_INET);

    /**
     * @brief 返回指定网卡的<网卡名, 地址, 子网掩码位数>
     * @param[in] result 保存结果
     * @param[in] iface 网卡名
     * @param[in] family 地址族(AF_INET, AF_INET6, AF_UNIX)
     * @return 成功返回true,失败返回false
     */
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result,
                                      const std::string&                              iface,
                                      int                                             family = AF_INET);

    virtual ~Address() {}

    // 获取地址族
    int getFamily() const;
    // 获取地址，返回sockaddr指针，只读
    virtual const sockaddr* getAddr() const = 0;
    // 获取地址，返回sockaddr指针，读写
    virtual sockaddr* getAddr() = 0;
    // 获取地址长度
    virtual socklen_t getAddrLen() const = 0;
    // 可读性输出地址
    virtual std::ostream& insert(std::ostream& os) const = 0;
    // 转换为string，并返回可读性字符串
    std::string toString() const;
    // 比较函数
    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;
};

// IP地址
class IPAddress : public Address {
public:
    typedef std::shared_ptr<IPAddress> ptr;

    // 通过域名、ip、服务器名创建IPAddress，端口号默认为0
    static IPAddress::ptr Create(const char* address, uint16_t port = 0);

    // 获取广播地址,
    // prefix_len为子网掩码位数，成功返回IPAddress::ptr，失败返回nullptr
    virtual IPAddress::ptr BroadcastAddress(uint32_t prefix_len) = 0;
    // 获取网络地址,
    // prefix_len为子网掩码位数，成功返回IPAddress::ptr，失败返回nullptr
    virtual IPAddress::ptr NetworkAddress(uint32_t prefix_len) = 0;
    // 获取子网掩码,
    // prefix_len为子网掩码位数，成功返回IPAddress::ptr，失败返回nullptr
    virtual IPAddress::ptr SubnetMask(uint32_t prefix_len) = 0;

    // 获取端口号
    virtual uint32_t getPort() const = 0;
    // 设置端口号
    virtual void setPort(uint16_t port) = 0;
};

// IPv4地址
class IPv4Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv4Address> ptr;

    /**
     * @brief 使用点分十进制地址创建IPv4Address
     * @param[in] address 点分十进制地址,如:192.168.1.1
     * @param[in] port 端口号
     * @return 返回IPv4Address,失败返回nullptr
     */
    static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

    /**
     * @brief 使用sockaddr_in创建IPv4Address
     * @param[in] address sockaddr_in结构体，包含IP地址和端口号
     */
    IPv4Address(const sockaddr_in& address);

    /**
     * @brief 通过二进制地址构造IPv4Address
     * @param[in] address 二进制地址
     * @param[in] port 端口号
     */
    IPv4Address(uint32_t host = INADDR_ANY, uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr*       getAddr() override;
    socklen_t       getAddrLen() const override;
    std::ostream&   insert(std::ostream& os) const override;

    IPAddress::ptr BroadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr NetworkAddress(uint32_t prefix_len) override;
    IPAddress::ptr SubnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void     setPort(uint16_t port) override;

private:
    sockaddr_in m_addr;
};


// IPv6地址
class IPv6Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv6Address> ptr;

    static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

    IPv6Address();
    IPv6Address(const sockaddr_in6& address);
    IPv6Address(const uint8_t address[16], uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr*       getAddr() override;
    socklen_t       getAddrLen() const override;
    std::ostream&   insert(std::ostream& os) const override;

    IPAddress::ptr BroadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr NetworkAddress(uint32_t prefix_len) override;
    IPAddress::ptr SubnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void     setPort(uint16_t port) override;

private:
    sockaddr_in6 m_addr;
};

// Unix套接字地址
class UnixAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr;

    UnixAddress();
    // 通过路径创建UnixAddress, 路径必须以/开头
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    sockaddr*       getAddr() override;
    socklen_t       getAddrLen() const override;
    void            setAddrLen(uint32_t addrlen);
    std::string     getPath() const;
    std::ostream&   insert(std::ostream& os) const override;


private:
    sockaddr_un m_addr;
    socklen_t   m_length;
};

// 未知地址
class UnknownAddress : public Address {
public:
    typedef std::shared_ptr<UnknownAddress> ptr;

    UnknownAddress(int family);
    UnknownAddress(const sockaddr& addr);

    const sockaddr* getAddr() const override;
    sockaddr*       getAddr() override;
    socklen_t       getAddrLen() const override;
    std::ostream&   insert(std::ostream& os) const override;

private:
    sockaddr m_addr;
};

// 流式输出Address
std::ostream& operator<<(std::ostream& os, const Address& addr);

}  // namespace sylar

#endif
