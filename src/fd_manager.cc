/**
 * @file fd_manager.cc
 * @brief 文件句柄管理类实现
 * @details 只管理socket fd，记录fd是否为socket，用户是否设置非阻塞，系统是否设置非阻塞，send/recv超时时间
 *          提供FdManager单例和get/del方法，用于创建/获取/删除fd
 * @date 2024-05-10
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/fd_manager.h"
#include "include/hook.h"

namespace sylar {

FdCtx::FdCtx(int fd)
    :m_isInit(false)
    ,m_isSocket(false)
    ,m_sysNonblock(false)
    ,m_userNonblock(false)
    ,m_isClosed(false)
    ,m_fd(fd)
    ,m_recvTimeout(-1)
    ,m_sendTimeout(-1) {
    init();
}

FdCtx::~FdCtx() {
}

bool FdCtx::init() {
    // 初始化过了就直接返回
    if(m_isInit) return true;
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    struct stat fd_stat;  // return 0 成功取出；-1 失败
    if(-1 == fstat(m_fd, &fd_stat)) {
        m_isInit = false;
        m_isSocket = false;
    } else {
        m_isInit = true;
        // 判断是否为socket
        m_isSocket = S_ISSOCK(fd_stat.st_mode);
    }
    // 如果是socket，设置非阻塞
    if(m_isSocket) {
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        // 如果用户没有设置非阻塞，设置非阻塞
        if(!(flags & O_NONBLOCK)) fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        m_sysNonblock = true;
    } else m_sysNonblock = false;

    m_userNonblock = false;
    m_isClosed = false;
    return m_isInit;
}

void FdCtx::setTimeout(int type, uint64_t v) {
    // 如果是读超时，设置读超时时间
    if(type == SO_RCVTIMEO) m_recvTimeout = v; 
    else m_sendTimeout = v;
}

uint64_t FdCtx::getTimeout(int type) {
    if(type == SO_RCVTIMEO) return m_recvTimeout;
    else return m_sendTimeout;
}

FdManager::FdManager() {
    m_datas.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
    if(fd == -1) return nullptr;
    // 集合中没有，并且不自动创建，返回nullptr
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_datas.size() <= fd) {
        if(!auto_create) return nullptr;
    } else {
        // 集合中有，直接返回
        if(m_datas[fd] || !auto_create) return m_datas[fd];
    }
    lock.unlock();

    RWMutexType::WriteLock lock2(m_mutex);
    // 创建新的FdCtx
    FdCtx::ptr ctx(new FdCtx(fd));
    // fd比集合下标大，需要扩容
    if(fd >= (int)m_datas.size()) m_datas.resize(fd * 1.5);
    // 将新的FdCtx放入集合中
    m_datas[fd] = ctx;
    return ctx;
}

void FdManager::del(int fd) {
    RWMutexType::WriteLock lock(m_mutex);
    if((int)m_datas.size() <= fd) return;
    m_datas[fd].reset();
}

}
