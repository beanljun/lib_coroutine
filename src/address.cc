/**
 * @file address.cc
 * @brief 地址模块
 * @author beanljun
 * @date 2024-09-23
 */

 #include <sys/socket.h>
 #include <netdb.h>
 #include <ifaddrs.h>


#include "include/address.h"
#include "include/log.h"
#include "include/endian.h"

namespace sylar {

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    // 创建掩码, 如bits=2, 则返回0xFFFFFC00
    template <class T>
    static T CreateMask(uint32_t bits) {
        return (1 << (sizeof(T) * 8 - bits)) - 1;
    }

    // 计算value中1的个数, 如value=0x0000000F, 则返回4
    template <class T>
    static uint32_t CountBytes(T value) {
        uint32_t result = 0;
        for (; value; ++result) {
            value &= value - 1;
        }
        return result;
    }

    Address::ptr Address::LookupAny(const std::string &host,
                                    int family, int type, int protocol) {
        std::vector<Address::ptr> result;
        
        if (Lookup(result, host, family, type, protocol)) {
            return result[0];
        }
        return nullptr;
    }

    IPAddress::ptr Address::LookupAnyIPAddress(const std::string &host,
                                            int family, int type, int protocol) {
        std::vector<Address::ptr> result;

        if (Lookup(result, host, family, type, protocol)) {
            for (auto& i : result) {
                IPAddress::ptr addr = std::dynamic_pointer_cast<IPAddress>(i);
                if (addr) {
                    return addr;
                }
            }
        }
        return nullptr;
    }
        
    bool Address::Lookup(std::vector<Address::ptr>& result, const std::string &host,
                        int family, int type, int protocol) {
        
        // 获取地址信息，hints是输入参数，results是输出参数，rp是遍历结果,
        addrinfo hints, *results, *rp; // rp全称result pointer
        hints.ai_flags = 0; // 标志位
        hints.ai_family = family; // 地址族
        hints.ai_socktype = type; // 套接字类型
        hints.ai_protocol = protocol; // 协议
        hints.ai_addrlen = 0; // 地址长度
        hints.ai_addr = nullptr; // 地址
        hints.ai_canonname = nullptr; // 规范名称
        hints.ai_next = nullptr; // 下一个地址

        std::string node;
        const char *service = nullptr;

        // 解析host, 如host=127.0.0.1:80, 则node=127.0.0.1, service=80
        if (node.empty()) {
            // memchr: 在字符串中查找字符，返回字符指针
            service = (const char*)memchr(host.c_str(), ':', host.size());
            if (service) {
                // 检查冒号后面是否还有其他冒号，                       -1为service指针本身
                if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                    node =  host.substr(0, service - host.c_str()); // 截取host的ip部分
                    ++service;
                }
            }
        }
        // 一通操作后，如果node为空，则node=host
        if (node.empty()) {
            node = host;
        }

        // 获取地址信息，失败返回-1，成功返回0
        int error = getaddrinfo(node.c_str(), service, &hints, &results);
        if (error) {
            SYLAR_LOG_DEBUG(g_logger) << "Address::Lookup getaddress(" << host << ", "
                            << family << ", " << type << ") err=" << error << " errstr="
                            << gai_strerror(error);
            return false;
        }

        rp = results;
        // 遍历结果，将结果添加到result中
        while (rp != nullptr) {
            result.push_back(Create(rp->ai_addr, static_cast<socklen_t>(rp->ai_addrlen)));
            // 一个ip/端口可以对应多种接字类型，比如SOCK_STREAM, SOCK_DGRAM, SOCK_RAW，所以这里会返回重复的结果
            SYLAR_LOG_DEBUG(g_logger) << "family:" << rp->ai_family << ", sock type:" << rp->ai_socktype;
            rp = rp->ai_next;
        }
        // 释放结果
        freeaddrinfo(results);
        return !result.empty();
    }

    bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>> &result,
                                        int family) {
        struct ifaddrs *next, *results;
        // 如果获取接口地址失败，返回false
        if (getifaddrs(&results) != 0) {
            SYLAR_LOG_DEBUG(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
                                        " err="
                                    << errno << " errstr=" << strerror(errno);
            return false;
        }
        try {
            // 遍历接口地址
            for (next = results; next != nullptr; next = next->ifa_next) {
                Address::ptr addr;
                uint32_t prefix_len = ~0u;
                if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                    continue;
                }
                // 根据地址族创建地址
                switch (next->ifa_addr->sa_family) {
                    // ipv4
                    case AF_INET: {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                        uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        prefix_len = CountBytes(netmask);
                    } break;
                    // ipv6
                    case AF_INET6: {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                        in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        prefix_len = 0;
                        for (int i = 0; i < 16; ++i) {
                            prefix_len += CountBytes(netmask.s6_addr[i]);
                        }
                    } break;
                    // 其他地址族
                    default:  
                        break;
                }
                // 如果addr不为空，则将addr和prefix_len添加到result中
                if (addr) {
                    result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_len)));
                }
            }
        } catch (...) {
            // 捕获所有异常，打印错误日志，释放结果，返回false
            SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
            freeifaddrs(results);
            return false;
        }
        freeifaddrs(results);
        return !result.empty();
    }
    
    bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>> &result,
                                        const std::string &iface, int family) {

        // 如果iface为空，或者iface为*，则返回所有接口的地址
        if (iface.empty() || iface == "*") {
            // 如果family为AF_INET（IPv4）或AF_UNSPEC（未指定），则返回IPv4地址
            if (family == AF_INET || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
            }
            // 如果family为AF_INET6（IPv6）或AF_UNSPEC（未指定），则返回IPv6地址
            if (family == AF_INET6 || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
            }
            return true;
        }

        // 如果iface不为空，则返回iface接口的地址
        std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;
        // 如果获取接口地址失败，返回false
        if (!GetInterfaceAddresses(results, family)) {
            return false;
        }

        // 遍历results，将结果添加到result中
        auto its = results.equal_range(iface);
        for ( ; its.first != its.second; ++its.first) {
            result.push_back(its.first->second);
        }

        return !result.empty();
    }

    int Address::getFamily() const {
        return getAddr()->sa_family;
    }

    std::string Address::toString() const {
        std::stringstream ss;
        insert(ss);
        return ss.str();
    }

    Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
        if (addr == nullptr) {
            return nullptr;
        }
        Address::ptr result;
        switch (addr->sa_family) {
            case AF_INET:
                result.reset(new IPv4Address(*reinterpret_cast<const sockaddr_in*>(addr)));
                break;
            case AF_INET6:
                result.reset(new IPv6Address(*reinterpret_cast<const sockaddr_in6*>(addr)));
                break;
            default:
                result.reset(new UnknownAddress(*addr));
                break;
        }
        return result;
    }

    bool Address::operator<(const Address& rhs) const {
        // 比较两个地址的前minlen个字节
        socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
        // memcmp: 比较两个内存区域的前minlen个字节, 返回值<0, 0, >0分别表示小于，等于，大于
        // 0x00000001 < 0x00000002
        // 0x00000001 == 0x00000001
        // 0x00000002 > 0x00000001
        int result = memcmp(getAddr(), rhs.getAddr(), minlen);
        if (result < 0) {
            return true;
        } else if (result > 0) {
            return false;
        } else if (getAddrLen() < rhs.getAddrLen()) {
            // 如果minlen相同，则比较地址长度
            return true;
        }
        return false;
    }

    bool Address::operator==(const Address& rhs) const {
        return getAddrLen() == rhs.getAddrLen()
            && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
    }

    bool Address::operator!=(const Address& rhs) const {
        return !(*this == rhs);
    }

    IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
        addrinfo hints, *results;
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_NUMERICHOST;    // 只支持数字型主机名
        hints.ai_family = AF_UNSPEC;         // 地址族为未指定

        int error = getaddrinfo(address, nullptr, &hints, &results);
        if (error) {
            SYLAR_LOG_DEBUG(g_logger) << "IPAddress::Create(" << address
                                    << ", " << port << ") error=" << error
                                    << " errno=" << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        // 创建IPAddress-将地址信息转换为IPAddress，并设置端口号
        try {
            IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
                Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
            if (result) {
                result->setPort(port);
            }
            freeaddrinfo(results);
            return result;
        } catch (...) {
            freeaddrinfo(results);
            return nullptr;
            }
    }

    // 创建IPv4地址
    IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
        IPv4Address::ptr rt(new IPv4Address());
        rt->m_addr.sin_port = byteswapOnLittleEndian(port);

        int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
        if (result <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "IPv4Address::Create(" << address << ", "
                                    << port << ") rt=" << result << " errno=" << errno
                                    << " errstr=" << strerror(errno);
            return nullptr;
        }
        return rt;
    }

    IPv4Address::IPv4Address(const sockaddr_in& address) {
        m_addr = address;
    }

    IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = byteswapOnLittleEndian(port);
        m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
    }

    const sockaddr* IPv4Address::getAddr() const {
        return (sockaddr*)&m_addr;
    }

    sockaddr* IPv4Address::getAddr() {
        return (sockaddr*)&m_addr;
    }

    socklen_t IPv4Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    // 格式化后的地址形式为 192.168.1.100:80
    std::ostream& IPv4Address::insert(std::ostream& os) const {
        uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
        os << ((addr >> 24) & 0xFF) << "."
           << ((addr >> 16) & 0xFF) << "."
           << ((addr >> 8) & 0xFF) << "."
           << (addr & 0xFF);
        os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
        return os;
    }

    // 将m_addr的sin_addr.s_addr与prefix_len位掩码进行或运算，得到广播地址
    IPAddress::ptr IPv4Address::BroadcastAddress(uint32_t prefix_len) {
        if (prefix_len > 32) {
            return nullptr;
        }

        sockaddr_in baddr(m_addr);
        // 将转换后的掩码与当前 IP 地址进行按位或操作：
        // 当前IP  ：  11000000 10101000 00000001 01100100 (192.168.1.100)
        // 掩码    ：  00000000 00000000 00000000 11111111
        // 或运算结果：11000000 10101000 00000001 11111111 (192.168.1.255)
        baddr.sin_addr.s_addr |= byteswapOnLittleEndian(
            CreateMask<uint32_t>(prefix_len));//创建掩码，如24位，二进制形式：11111111 11111111 11111111 00000000
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    // 创建网络地址，如192.168.1.100/24，网络地址为192.168.1.0
    IPAddress::ptr IPv4Address::NetworkAddress(uint32_t prefix_len) {
        if (prefix_len > 32) {
            return nullptr;
        }

        sockaddr_in baddr(m_addr);
        // 当前 IP：       11000000 10101000 00000001 01100100 (192.168.1.100)
        // 反转掩码：       11111111 11111111 11111111 00000000
        // 与运算结果：     11000000 10101000 00000001 00000000 (192.168.1.0)
        baddr.sin_addr.s_addr &= byteswapOnLittleEndian(
        ~CreateMask<uint32_t>(prefix_len));
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    // 创建子网掩码，如192.168.1.100/24，子网掩码为255.255.255.0
    IPAddress::ptr IPv4Address::SubnetMask(uint32_t prefix_len) {
        sockaddr_in subnet;
        memset(&subnet, 0, sizeof(subnet));
        subnet.sin_family      = AF_INET;
        // 创建掩码：       00000000 00000000 00000000 11111111
        // 取反：          11111111 11111111 11111111 00000000
        subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
        return IPv4Address::ptr(new IPv4Address(subnet));
    }

    uint32_t IPv4Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin_port);
    }

    void IPv4Address::setPort(uint16_t v) {
        m_addr.sin_port = byteswapOnLittleEndian(v);
    }

    IPv6Address::ptr IPv6Address::Create(const char *address, uint16_t port) {
        IPv6Address::ptr rt(new IPv6Address);
        rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
        int result           = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
        if (result <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "IPv6Address::Create(" << address << ", "
                                    << port << ") rt=" << result << " errno=" << errno
                                    << " errstr=" << strerror(errno);
            return nullptr;
        }
        return rt;
    }

    IPv6Address::IPv6Address() {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
    }

    IPv6Address::IPv6Address(const sockaddr_in6 &address) {
        m_addr = address;
    }

    IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
        m_addr.sin6_port   = byteswapOnLittleEndian(port);
        memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
    }

    sockaddr *IPv6Address::getAddr() {
        return (sockaddr *)&m_addr;
    }

    const sockaddr *IPv6Address::getAddr() const {
        return (sockaddr *)&m_addr;
    }

    socklen_t IPv6Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    // 2001:0db8:85a3:0000:0000:8a2e:0370:7334端口8080 -> [2001:db8:85a3::8a2e:370:7334]:8080
    std::ostream &IPv6Address::insert(std::ostream &os) const {
        os << "[";
        uint16_t *addr  = (uint16_t *)m_addr.sin6_addr.s6_addr;
        bool used_zeros = false;
        for (size_t i = 0; i < 8; ++i) {
            if (addr[i] == 0 && !used_zeros) {
                continue;
            }
            if (i && addr[i - 1] == 0 && !used_zeros) {
                os << ":";
                used_zeros = true;
            }
            if (i) {
                os << ":";
            }
            os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
        }

        if (!used_zeros && addr[7] == 0) {
            os << "::";
        }

        os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
        return os;
    }

    IPAddress::ptr IPv6Address::BroadcastAddress(uint32_t prefix_len) {
        sockaddr_in6 baddr(m_addr);
        baddr.sin6_addr.s6_addr[prefix_len / 8] |=
            CreateMask<uint8_t>(prefix_len % 8);
        for (int i = prefix_len / 8 + 1; i < 16; ++i) {
            baddr.sin6_addr.s6_addr[i] = 0xff;
        }
        return IPv6Address::ptr(new IPv6Address(baddr));
    }

    IPAddress::ptr IPv6Address::NetworkAddress(uint32_t prefix_len) {
        sockaddr_in6 baddr(m_addr);
        baddr.sin6_addr.s6_addr[prefix_len / 8] &=
            CreateMask<uint8_t>(prefix_len % 8);
        for (int i = prefix_len / 8 + 1; i < 16; ++i) {
            baddr.sin6_addr.s6_addr[i] = 0x00;
        }
        return IPv6Address::ptr(new IPv6Address(baddr));
    }

    IPAddress::ptr IPv6Address::SubnetMask(uint32_t prefix_len) {
        sockaddr_in6 subnet;
        memset(&subnet, 0, sizeof(subnet));
        subnet.sin6_family = AF_INET6;
        subnet.sin6_addr.s6_addr[prefix_len / 8] =
            ~CreateMask<uint8_t>(prefix_len % 8);

        for (uint32_t i = 0; i < prefix_len / 8; ++i) {
            subnet.sin6_addr.s6_addr[i] = 0xff;
        }
        return IPv6Address::ptr(new IPv6Address(subnet));
    }

    uint32_t IPv6Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin6_port);
    }

    void IPv6Address::setPort(uint16_t v) {
        m_addr.sin6_port = byteswapOnLittleEndian(v);
    }

    static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un *)0)->sun_path) - 1;

    UnixAddress::UnixAddress() {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length          = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
    }

    UnixAddress::UnixAddress(const std::string &path) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length          = path.size() + 1;

        if (!path.empty() && path[0] == '\0') {
            --m_length;
        }

        if (m_length > sizeof(m_addr.sun_path)) {
            throw std::logic_error("path too long");
        }
        memcpy(m_addr.sun_path, path.c_str(), m_length);
        m_length += offsetof(sockaddr_un, sun_path);
    }

    void UnixAddress::setAddrLen(uint32_t v) {
        m_length = v;
    }

    sockaddr *UnixAddress::getAddr() {
        return (sockaddr *)&m_addr;
    }

    const sockaddr *UnixAddress::getAddr() const {
        return (sockaddr *)&m_addr;
    }

    socklen_t UnixAddress::getAddrLen() const {
        return m_length;
    }

    std::string UnixAddress::getPath() const {
        std::stringstream ss;
        if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
            ss << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
        } else {
            ss << m_addr.sun_path;
        }
        return ss.str();
    }

    std::ostream &UnixAddress::insert(std::ostream &os) const {
        if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
            return os << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
        }
        return os << m_addr.sun_path;
    }

    UnknownAddress::UnknownAddress(int family) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sa_family = family;
    }

    UnknownAddress::UnknownAddress(const sockaddr &addr) {
        m_addr = addr;
    }

    sockaddr *UnknownAddress::getAddr() {
        return (sockaddr *)&m_addr;
    }

    const sockaddr *UnknownAddress::getAddr() const {
        return &m_addr;
    }

    socklen_t UnknownAddress::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream &UnknownAddress::insert(std::ostream &os) const {
        os << "[UnknownAddress family=" << m_addr.sa_family << "]";
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const Address &addr) {
        return addr.insert(os);
    }

} // namespace sylar