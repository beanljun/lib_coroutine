/**
 * @file iomanager.cc
 * @brief IO协程调度器实现
 * @author beanljun
 * @date 2024-5-08
*/

#include <unistd.h>
#include <fcntl.h> 
#include <cstring>
#include <sys/epoll.h>

#include "include/iomanager.h"
#include "include/log.h"
#include "include/macro.h"

namespace sylar {

enum EpollCtlOp {
};

static std::ostream& operator<<(std::ostream& os, const EpollCtlOp& op) {
    switch ((int)op) {
#define XX(ctl) \
    case ctl: \
        return os << #ctl;
        XX(EPOLL_CTL_ADD);
        XX(EPOLL_CTL_MOD);
        XX(EPOLL_CTL_DEL);
#undef XX
    default:
        return os << (int)op;
    }
}

static std::ostream& operator<<(std::ostream& os, EPOLL_EVENTS& events) {
    if (!events) {
        return os << "0";
    }
    bool first = true;
#define XX(E) \
    if (events & E) { \
        if (!first) { \
            os << "|"; \
        } \
        os << #E; \
        first = false; \
    }
    XX(EPOLLIN);
    XX(EPOLLPRI);
    XX(EPOLLOUT);
    XX(EPOLLRDNORM);
    XX(EPOLLRDBAND);
    XX(EPOLLWRNORM);
    XX(EPOLLWRBAND);
    XX(EPOLLMSG);
    XX(EPOLLERR);
    XX(EPOLLHUP);
    XX(EPOLLRDHUP);
    XX(EPOLLONESHOT);
    XX(EPOLLET);
#undef XX
    return os;
}

IOManager::FdContext::EventContext& IOManager::FdContext::getEventContext(IOManager::Event event) {
    switch (event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            SYLAR_ASSERT2(false, "getEventContext");
    }
    throw std::invalid_argument("getEventContext invalid event");   // 不会执行到这里
}

void IOManager::FdContext::resetEventContext(EventContext& ctx) {
    ctx.scheduler = nullptr;    // 重置调度器
    ctx.fiber.reset();          // 重置协程
    ctx.cb = nullptr;           // 重置回调函数
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
    SYLAR_ASSERT(events & event);        // 待触发的事件必须已被注册过

    // 清除该事件，表示不再关注该事件了
    // 也就是说，注册的IO事件是一次性的，如果想持续关注某个socket fd的读写事件，那么每次触发事件之后都要重新添加
    events = (Event)(events & ~event); 

    EventContext& ctx = getEventContext(event);         // 获取事件上下文
    if (ctx.cb)   ctx.cb();                             // 如果有回调函数，直接调用回调函数
    else          ctx.scheduler -> schedule(ctx.fiber);   // 否则，调度器调度协程

    resetEventContext(ctx);                             // 重置事件上下文
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    : Scheduler(threads, use_caller, name) {
    m_epfd = epoll_create(5000);    // 创建epoll句柄, 参数为epoll监听的fd的数量
    SYLAR_ASSERT(m_epfd > 0);       // 断言创建成功

    int rt = pipe(m_tickleFds);    // 创建管道
    SYLAR_ASSERT(!rt);

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));// 初始化epoll事件
    event.events = EPOLLIN | EPOLLET;   // 边缘触发
    event.data.fd = m_tickleFds[0];     // 管道的读端

    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);   // 设置非阻塞
    SYLAR_ASSERT(!rt);

    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);   // 添加管道读端的事件
    SYLAR_ASSERT(!rt);

    contextResize(32);  
    start();
}

IOManager::~IOManager() {
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (m_fdContexts[i]) delete m_fdContexts[i];
    }
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    // 遍历所有fd，如果fd对应的上下文不存在，则创建一个
    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i] -> fd = i;
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    // 找到fd对应的上下文，如果不存在，那么就分配一个
    FdContext* fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() > fd) {    
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } else {                                
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    //  同一个fd不允许重复添加相同的事件
    FdContext::MutexType::Lock lock2(fd_ctx -> mutex);
    if (SYLAR_UNLIKELY(fd_ctx -> events & event)) {
        SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                                  << " event=" << (EPOLL_EVENTS)event
                                  << " fd_ctx.event=" << (EPOLL_EVENTS)fd_ctx -> events;
        SYLAR_ASSERT(!(fd_ctx -> events & event));
    }

    // 将新的事件加入epoll_wait，使用epoll_event的私有指针存储FdContext的位置
    int op = fd_ctx -> events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;  // 如果已经有事件了，那么就是修改事件，否则就是添加事件
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx -> events | event;        // 边缘触发，保留原有事件，添加新事件
    epevent.data.ptr = fd_ctx;                                  // epoll_event的私有指针存储FdContext的位置

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);               // 添加或修改事件
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                                  << rt << " (" << errno << ") (" << strerror(errno) << ") fd_ctx->events="
                                  << (EPOLL_EVENTS)fd_ctx->events;
        return -1;
    }

    ++m_pendingEventCount;  // 增加待处理事件数量

     // 找到这个fd的event事件对应的EventContext，对其中的scheduler, cb, fiber进行赋值
    fd_ctx -> events = (Event)(fd_ctx -> events | event);                   // 更新fd_ctx的事件
    FdContext::EventContext& event_ctx = fd_ctx -> getEventContext(event);  // 获取事件上下文
    SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);// 断言事件上下文中的调度器，协程，回调函数都为空

    // 赋值scheduler和回调函数，如果回调函数为空，则把当前协程当成回调执行体
    event_ctx.scheduler = Scheduler::GetThis();     // 将当前调度器赋值给事件上下文的调度器
    if (cb)     event_ctx.cb.swap(cb);              // 交换回调函数 
    else {
        event_ctx.fiber = Fiber::GetThis();         // 获取当前协程
        SYLAR_ASSERT2(event_ctx.fiber -> getState() == Fiber::RUNNING, "state=" << event_ctx.fiber -> getState());
    }
    return 0;

}

bool IOManager::delEvent(int fd, Event event) {
    // 找到fd对应的上下文 fdcontext
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd)  return false; // 如果fd对应的上下文不存在，那么直接返回false

    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx -> mutex);
    if (SYLAR_UNLIKELY(!(fd_ctx -> events & event)))  return false; // 如果fd对应的上下文中没有这个事件，那么直接返回false

    // 清除指定的事件，表示不关心这个事件了，如果清除之后结果为0，则从epoll_wait中删除该文件描述符
    Event new_events = (Event)(fd_ctx -> events & ~event);  // 清除指定的事件
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;    // 如果还有事件，那么就是修改事件，否则就是删除事件
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;                  
    epevent.data.ptr = fd_ctx;                              

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);   
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                                  << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;  // 减少待处理事件数量

    // 重置该fd对应的event事件上下文
    fd_ctx -> events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx -> getEventContext(event);
    fd_ctx -> resetEventContext(event_ctx);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    // 找到fd对应的上下文
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd)  return false;

    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx -> mutex);
    if (SYLAR_UNLIKELY(!(fd_ctx -> events & event)))  return false;

    // 清除指定的事件，表示不关心这个事件了
    Event new_events = (Event)(fd_ctx -> events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                                  << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    fd_ctx->triggerEvent(event);  // 清除之前触发一次事件
    --m_pendingEventCount;        // 减少待处理事件数量
    return true;
}

bool IOManager::cancelAll(int fd) {
    // 找到fd对应的上下文
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd)  return false;

    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx -> mutex);
    if (!fd_ctx -> events)  return false;

    // 清除所有事件
    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                                  << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    // 触发全部已注册的事件
    if (fd_ctx -> events & READ) {
        fd_ctx -> triggerEvent(READ); 
        --m_pendingEventCount;
    }
    if (fd_ctx -> events & WRITE) {
        fd_ctx -> triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    SYLAR_ASSERT(fd_ctx -> events == 0);
    return true;
}

/// TODO





} // namespace sylar