/**
 * @file tcp_server.h
 * @brief TCP服务器类
 * @author beanljun
 * @date 2024-09-23
 */

#ifndef __SYLAR_TCP_SERVER_H__
#define __SYLAR_TCP_SERVER_H__

#include <memory>

#include "address.h"
#include "socket.h"
#include "noncopyable.h"
#include "iomanager.h"
// #include "log.h"

namespace sylar {

    class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
    public:
        typedef std::shared_ptr<TcpServer> ptr;
        
        /**
         * @brief 构造函数
         * @param[in] worker socket客户端工作的协程调度器
         * @param[in] accept_worker 服务器socket执行接收socket连接的协程调度器
         */
        TcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                  sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
        virtual ~TcpServer();
        
        // 绑定地址，返回是否绑定成功
        virtual bool bind(sylar::Address::ptr addr);

        // 绑定地址数组，返回是否绑定成功，以及失败数组
        virtual bool bind(const std::vector<Address::ptr>& addrs,
                          std::vector<Address::ptr>& fails);
        
        // 启动服务器，需要先bind
        virtual bool start();

        // 停止服务器
        virtual void stop();

        // 获取服务器名称
        std::string getName() const { return m_name; }

        // 获取读取超时时间
        uint64_t getRecvTimeout() const { return m_recvTimeout; }

        // 设置服务器名称
        void setName(const std::string& name) { m_name = name; }

        // 设置读取超时时间
        void setRecvTimeout(uint64_t v) { m_recvTimeout = v; }

        // 检查服务器是否停止
        bool isStop() const { return m_isStop; }
        
        // 以字符串形式输出服务器信息
        virtual std::string toString(const std::string& prefix = "");

    protected:
        // 处理新连接的socket类
        virtual void handleClient(Socket::ptr client);

        // 开始接受连接
        virtual void startAccept(Socket::ptr sock);

    protected:
        std::vector<Socket::ptr> m_socks;  // 监听Socket数组
        IOManager* m_worker;               // 新连接的Socket工作的调度器
        IOManager* m_acceptWorker;         // 服务器Socket接收连接的调度器
        uint64_t m_recvTimeout;            // 接收超时时间(毫秒)
        std::string m_name;                // 服务器名称
        std::string m_type;                // 服务器类型
        bool m_isStop;                     // 服务是否停止
        
    };
}

#endif


