#include "include/log.h"
#include "include/config.h"
#include "include/tcp_server.h"

namespace sylar {

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    // 配置项：tcp_server.read_timeout
    static sylar::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout = 
        sylar::Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");

    TcpServer::TcpServer(IOManager* worker, IOManager* accept_worker)
        : m_worker(worker), m_acceptWorker(accept_worker)
        , m_recvTimeout(g_tcp_server_read_timeout->getValue())
        , m_name("sylar/1.0.0/tcp_server")
        , m_type("tcp_server") , m_isStop(true) { }

    // 清理监听的Socket
    TcpServer::~TcpServer() {
        for(auto& i : m_socks) {
            i->close();
        }
        m_socks.clear();
    }

    bool TcpServer::bind(sylar::Address::ptr addr) {
        std::vector<Address::ptr> addrs;
        std::vector<Address::ptr> fails;
        addrs.emplace_back(addr);
        return bind(addrs, fails);
    }

    bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails) {
        for(auto& addr : addrs) {
            // 为取到addr创建Socket
            Socket::ptr sock = Socket::CreateTCP(addr);
            if(!sock->bind(addr)) { // 绑定地址
                SYLAR_LOG_ERROR(g_logger) << "bind fail: " << addr->toString();
                fails.emplace_back(addr);
                continue;
            }
            // 判断是否监听成功
            if(!sock->listen()) { 
                SYLAR_LOG_ERROR(g_logger) << "listen fail: " << sock->toString();
                fails.emplace_back(addr);
                continue;
            }
            // 将监听状态的Socket添加到数组中
            m_socks.emplace_back(sock);
        }
        // 如果绑定失败或者监听失败，则返回false
        if(!fails.empty()) {
            m_socks.clear();
            return false;
        }
        // 打印绑定成功的Socket
        for(auto& i : m_socks) {
            SYLAR_LOG_INFO(g_logger) << "type=" << m_type << " name=" << m_name
                << " server bind success: " << *i;
        }
        return true;
    }
    
    void TcpServer::startAccept(Socket::ptr sock) {
        // 服务器未停止，一直接受连接
        while(!m_isStop) {
            // 接受连接
            Socket::ptr client = sock->accept();
            if(client) { // 接受成功
                client->setRecvTimeout(m_recvTimeout);
                // 将连接的Socket交给调度器处理
                m_worker->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
            } else { // 接受失败，打印错误信息
                SYLAR_LOG_ERROR(g_logger) << "accept error: " << errno
                    << " errstr=" << strerror(errno);
            }
        }
    }

    bool TcpServer::start() {
        if(!m_isStop) { // 服务已启动
            return true;
        }
        m_isStop = false;
        for(auto& i : m_socks) {
            // 将监听的Socket交给调度器处理
            m_acceptWorker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), i));
        }
        return true;
    }

    void TcpServer::stop() {
        m_isStop = true;
        auto self = shared_from_this();
        // 通过lambda表达式停止调度器
        m_acceptWorker->schedule([this, self]() {
            for(auto& sock : m_socks) {
                sock->cancelAll();
                sock->close();
            }
            m_socks.clear();
        });
    }

    void TcpServer::handleClient(Socket::ptr client) {
        SYLAR_LOG_INFO(g_logger) << "handleClient: " << *client;
    }

    std::string TcpServer::toString(const std::string& prefix) {
        std::stringstream ss;
        // 输出服务器信息
        ss << prefix << "[type=" << m_type
            << " name=" << m_name
            << " io_worker=" << (m_worker ? m_worker->getName() : "")
            << " accept=" << (m_acceptWorker ? m_acceptWorker->getName() : "")
            << " recv_timeout=" << m_recvTimeout << "]" << std::endl;
        // 输出监听的Socket信息
        for(auto& i : m_socks) {
            ss << prefix << "  " << *i << std::endl;
        }
        return ss.str();
    }

} // namespace sylar
