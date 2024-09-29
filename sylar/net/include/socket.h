/*
 * @file socket.h
 * @author beanljun
 * @brief socket类
 * @date 2024-09-26
 */

#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <memory>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "address.h"
#include "../../util/noncopyable.h"

namespace sylar {

class Socket : public std::enable_shared_from_this<Socket> , Noncopyable {
public:
    using ptr = std::shared_ptr<Socket>;
    using weak_ptr = std::weak_ptr<Socket>;

    // socket类型
    enum Type {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };

    // socket地址类型
    enum Family {
        IPv4 = AF_INET,
        IPv6 = AF_INET6,
        UNIX = AF_UNIX
    };

    // 创建TCP socket
    static Socket::ptr CreateTCP(sylar::Address::ptr address);
    // 创建UDP socket
    static Socket::ptr CreateUDP(sylar::Address::ptr address);

    // ipv4的TCP socket
    static Socket::ptr CreateTCPSocket();
    // ipv4的UDP socket
    static Socket::ptr CreateUDPSocket();
    // ipv6的TCP socket
    static Socket::ptr CreateTCPSocket6();
    // ipv6的UDP socket
    static Socket::ptr CreateUDPSocket6();

    // 创建UNIX TCP socket
    static Socket::ptr CreateUnixTCPSocket();
    // 创建UNIX UDP socket
    static Socket::ptr CreateUnixUDPSocket();

    /**
     * @brief 构造函数
     * @param family 地址族，如AF_INET
     * @param type socket类型，如SOCK_STREAM
     * @param protocol 协议，如IPPROTO_TCP
     */
    Socket(int family, int type, int protocol = 0);
    // 析构函数
    virtual ~Socket();

    // 获取发送超时时间,单位ms
    int64_t getSendTimeout();
    // 设置发送超时时间,单位ms
    void setSendTimeout(int64_t v);
    // 获取接收超时时间,单位ms
    int64_t getRecvTimeout();
    // 设置接收超时时间,单位ms
    void setRecvTimeout(int64_t v);

    // 获取socket地址
    bool getOption(int level, int option, void* result, socklen_t* len);

    // 获取socket 模板
    template<class T>
    bool getOption(int level, int option, T& value) {
        socklen_t len = sizeof(T);
        return getOption(level, option, &value, &len);
    }
    
    // 设置socket 
    bool setOption(int level, int option, const void* value, socklen_t len);
    // 设置socket 模板
    template<class T>
    bool setOption(int level, int option, const T& value) {
        return setOption(level, option, &value, sizeof(T));
    }

    /**
     * @brief 接受一个connect连接
     * @return 成功返回一个新的Socket对象，失败返回nullptr
     * @pre Socket必须 bind , listen 成功
     */
    virtual Socket::ptr accept();
    // 绑定地址
    virtual bool bind(const Address::ptr address);
    // 连接
    virtual bool connect(const Address::ptr address, uint64_t timeout_ms = -1);
    // 重新连接
    virtual bool reconnect(uint64_t timeout_ms = -1);
    /**
     * @brief 监听
     * @param backlog 未完成连接队列的最大长度
     * @return 成功返回true，失败返回false
     * @pre 必须先 bind 成功
     */
     virtual bool listen(int backlog = SOMAXCONN);
    // 关闭socket
    virtual bool close();

    /**
     * @brief 发送数据
     * @param buffer 发送缓冲区
     * @param length 发送长度
     * @param flags 标志位
     * @return >0 发送的字节数，=0 对方关闭，<0 发送失败
     */
    virtual int send(const void* buffer, size_t length, int flags = 0);
    
    /**
     * @brief 发送数据
     * @param buffers 发送缓冲区数组(iovec数组)
     * @param length 发送长度(iovec长度)
     * @param flags 标志位
     * @return >0 发送的字节数，=0 对方关闭，<0 发送失败
     */
    virtual int send(const iovec* buffers, size_t length, int flags = 0);

    /**
     * @brief 发送数据到指定地址
     * @param buffer 发送缓冲区
     * @param length 发送长度
     * @param to     目标地址
     * @param flags 标志位
     * @return >0 发送的字节数，=0 对方关闭，<0 发送失败
     */
    virtual int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);

    /**
     * @brief 发送数据到指定地址
     * @param buffers 发送缓冲区数组(iovec数组)
     * @param length 发送长度(iovec长度)
     * @param to     目标地址
     * @param flags 标志位
     * @return >0 发送的字节数，=0 对方关闭，<0 发送失败
     */
    virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);

    /**
     * @brief 接收数据
     * @param buffer 接收缓冲区
     * @param length 接收长度
     * @param flags 标志位
     * @return >0 接收的字节数，=0 对方关闭，<0 接收失败
     */
    virtual int recv(void* buffer, size_t length, int flags = 0);

    /**
     * @brief 接收数据
     * @param buffers 接收缓冲区数组(iovec数组)
     * @param length 接收长度(iovec长度)
     * @param flags 标志位
     * @return >0 接收的字节数，=0 对方关闭，<0 接收失败
     */
    virtual int recv(iovec* buffers, size_t length, int flags = 0);

    /**
     * @brief 接收数据
     * @param buffer 接收缓冲区
     * @param length 接收长度
     * @param from   源地址
     * @param flags 标志位
     * @return >0 接收的字节数，=0 对方关闭，<0 接收失败
     */
    virtual int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);

    /**
     * @brief 接收数据
     * @param buffers 接收缓冲区数组(iovec数组)
     * @param length 接收长度(iovec长度)
     * @param from   源地址
     * @param flags 标志位
     * @return >0 接收的字节数，=0 对方关闭，<0 接收失败
     */
    virtual int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

    // 获取远端地址
    Address::ptr getRemoteAddress();
    // 获取本地地址
    Address::ptr getLocalAddress();

    // 获取协议族
    int getFamily() const { return m_family; }
    // 获取socket类型
    int getType() const { return m_type; }
    // 获取协议
    int getProtocol() const { return m_protocol; }

    // 返回是否连接
    bool isConnected() const { return m_isConnected; }
    // 是否有效 (m_sock != -1)
    bool isValid() const;
    // 返回Socket错误
    int getError();
    // 返回Socket描述符
    int getSocket() const { return m_sock; }
    
    // 取消读
    bool cancelRead();
    // 取消写
    bool cancelWrite();
    // 取消accept
    bool cancelAccept();
    // 取消所有
    bool cancelAll();

    // 输出信息到流中
    virtual std::ostream &dump(std::ostream &os) const;
    // 转换为字符串
    virtual std::string toString() const;

protected:
    // 初始化socket
    void initSock();

    // 创建socket
    void newSock();

    // 初始化sock
    virtual bool init(int sock);

protected:
    int m_sock;             // socket描述符
    int m_family;           // 协议族
    int m_type;             // socket类型
    int m_protocol;         // 协议
    bool m_isConnected;    // 是否连接
    Address::ptr m_localAddress; // 本地地址
    Address::ptr m_remoteAddress; // 远端地址
};

// 输出socket信息到流中
std::ostream& operator<<(std::ostream& os, const Socket& sock);

}

#endif
