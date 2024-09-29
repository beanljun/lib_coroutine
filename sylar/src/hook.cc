#include <dlfcn.h>

#include "../util/macro.h"
#include "../include/log.h"
#include "../include/hook.h"
#include "../include/fiber.h"
#include "../include/config.h"
#include "../include/iomanager.h"
#include "../include/fd_manager.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sylar {

static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout =
    sylar::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hook_init() {
    static bool is_inited = false;
    if(is_inited) {
        return;
    }
// dlsym - 从一个动态链接库或者可执行文件中获取到符号地址。成功返回跟name关联的地址
// RTLD_NEXT 返回第一个匹配到的 "name" 的函数地址
// 取出原函数，赋值给新函数
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;
struct _HookIniter {
    _HookIniter() {
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout -> getValue();

        g_tcp_connect_timeout -> addListener([](const int& old_value, const int& new_value){
                SYLAR_LOG_INFO(g_logger) << "tcp connect timeout changed from "
                                         << old_value << " to " << new_value;
                s_connect_timeout = new_value;
        });
    }
};

static _HookIniter s_hook_initer;

bool is_hook_enable() {
    return t_hook_enable;
}

void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

}

struct timer_info {
    int cancelled = 0;
};

template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name,
        uint32_t event, int timeout_so, Args&&... args) {
    // 如果不hook，直接返回原接口
    if(!sylar::t_hook_enable) {
        // forward完美转发，可以将传入的可变参数args以原始类型的方式传递给函数fun。
        // 这样做的好处是可以避免不必要的类型转换和拷贝，提高代码的效率和性能。
        return fun(fd, std::forward<Args>(args)...);
    }
    // 获取fd对应的FdCtx
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
    if(!ctx) {  // 没有文件 
        return fun(fd, std::forward<Args>(args)...);
    }
    // 检查句柄是否关闭
    if(ctx -> isClose()) {
        errno = EBADF;  // 错误码
        return -1;
    }
    // 如果不是socket或者用户设置了非阻塞，直接调用原始函数
    if(!ctx -> isSocket() || ctx -> getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx -> getTimeout(timeout_so);      // 获取超时时间
    std::shared_ptr<timer_info> tinfo(new timer_info);// 设置超时条件

retry:
    // 先调用原始函数读数据或写数据 若函数返回值有效就直接返回
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    SYLAR_LOG_DEBUG(g_logger) << "do_io <" << hook_fun_name << ">" << " n = " << n;
    // 若中断则重试
    while(n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }
    // 若为阻塞状态
    if(n == -1 && errno == EAGAIN) {
        sylar::IOManager* iom = sylar::IOManager::GetThis();// 获取IOManager
        sylar::Timer::ptr timer;    // 定时器
        std::weak_ptr<timer_info> winfo(tinfo); // tinfo的弱指针，可以判断tinfo是否已经销毁
        // 若超时时间不为-1，则设置定时器
        if(to != (uint64_t)-1) {
            // 添加条件定时器 —— to时间消息还没来就触发callback 
            timer = iom -> addConditionTimer(to, [winfo, fd, iom, event]() {
                auto t = winfo.lock();
                // tinfo失效 || 设了错误   定时器失效了 
                if(!t || t -> cancelled) return;

                t -> cancelled = ETIMEDOUT; // 没错误的话设置为超时而失败
                // 取消事件强制唤醒
                iom -> cancelEvent(fd, (sylar::IOManager::Event)(event));
            }, winfo);
        }
        // 添加事件
        int rt = iom -> addEvent(fd, (sylar::IOManager::Event)(event));
        // 添加事件失败
        if(SYLAR_UNLIKELY(rt)) {
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
                << fd << ", " << event << ")";
            if(timer) timer -> cancel(); // 取消定时器
            return -1;
        } else {
            /*	addEvent成功，把执行时间让出来
             *	只有两种情况会从这回来：
             * 	1) 超时了， timer cancelEvent triggerEvent会唤醒回来
             * 	2) addEvent数据回来了会唤醒回来 */
            sylar::Fiber::GetThis() -> yield();
            if(timer) timer -> cancel();    // 回来了还有定时器就取消掉

            if(tinfo -> cancelled) {     // 从定时任务唤醒，超时失败
                errno = tinfo -> cancelled;
                return -1;
            }
            // 数据来了就直接重新去操作
            goto retry;
        }
    }
    
    return n;
}

// 声明变量
extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds) {
    if(!sylar::t_hook_enable) {
        return sleep_f(seconds);
    }

    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    /*
     *	(void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread)) 是一个函数指针类型，
     *	它定义了一个指向 sylar::Scheduler 类中一个参数为 sylar::Fiber::ptr 和 int 类型的成员函数的指针类型。
     *	具体来说，它的含义如下：
     *	void 表示该成员函数的返回值类型，这里是 void 类型。
     *	(sylar::Scheduler::*) 表示这是一个 sylar::Scheduler 类的成员函数指针类型。
     *	(sylar::Fiber::ptr, int thread) 表示该成员函数的参数列表
     *       ，其中第一个参数为 sylar::Fiber::ptr 类型，第二个参数为 int 类型。
     *	
     *	使用 std::bind 绑定了 sylar::IOManager::schedule 函数，
     * 	并将 iom 实例作为第一个参数传递给了 std::bind 函数，将sylar::IOManager::schedule函数绑定到iom对象上。
     * 	在这里，第二个参数使用了函数指针类型 (void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread))
     * 	，表示要绑定的函数类型是 sylar::Scheduler 类中一个参数为 sylar::Fiber::ptr 和 int 类型的成员函数
     * 	，这样 std::bind 就可以根据这个函数类型来实例化出一个特定的函数对象，并将 fiber 和 -1 作为参数传递给它。
     */
    iom -> addTimer(seconds * 1000, std::bind((void(sylar::Scheduler::*)
            (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
            ,iom, fiber, -1));
    sylar::Fiber::GetThis() -> yield();
    return 0;
}

int usleep(useconds_t usec) {
    if(!sylar::t_hook_enable) {
        return usleep_f(usec);
    }
    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom -> addTimer(usec / 1000, std::bind((void(sylar::Scheduler::*)
            (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
            ,iom, fiber, -1));
    sylar::Fiber::GetThis() -> yield();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if(!sylar::t_hook_enable) {
        return nanosleep_f(req, rem);
    }

    int timeout_ms = req -> tv_sec * 1000 + req -> tv_nsec / 1000 /1000;
    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom -> addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)
            (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
            ,iom, fiber, -1));
    sylar::Fiber::GetThis() -> yield();
    return 0;
}

int socket(int domain, int type, int protocol) {
    if(!sylar::t_hook_enable) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if(fd == -1) return fd;
    // 将fd放入到文件管理中
    sylar::FdMgr::GetInstance() -> get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
    if(!sylar::t_hook_enable) {
        return connect_f(fd, addr, addrlen);
    }
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
    if(!ctx || ctx -> isClose()) {
        errno = EBADF;
        return -1;
    }

    if(!ctx -> isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if(ctx -> getUserNonblock()) {
        return connect_f(fd, addr, addrlen);
    }

    // ----- 异步开始 -----
    // 先尝试连接
    int n = connect_f(fd, addr, addrlen);
    // 连接成功
    if(n == 0) {
        return 0;
    // 其他错误，EINPROGRESS表示连接操作正在进行中
    } else if(n != -1 || errno != EINPROGRESS) {
        return n;
    }

    sylar::IOManager* iom = sylar::IOManager::GetThis();
    sylar::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    // 设置了超时时间
    if(timeout_ms != (uint64_t)-1) {
        // 添加条件定时器
        timer = iom -> addConditionTimer(timeout_ms, [winfo, fd, iom]() {
                auto t = winfo.lock();
                if(!t || t -> cancelled) {
                    return;
                }
                t -> cancelled = ETIMEDOUT;
                iom -> cancelEvent(fd, sylar::IOManager::WRITE);
        }, winfo);
    }
    // 添加一个写事件
    int rt = iom -> addEvent(fd, sylar::IOManager::WRITE);
    if(rt == 0) {
        /* 	只有两种情况唤醒：
         * 	1. 超时，从定时器唤醒
         *	2. 连接成功，从epoll_wait拿到事件 */
        sylar::Fiber::GetThis() -> yield();
        if(timer) {
            timer -> cancel();
        }
        // 从定时器唤醒，超时失败
        if(tinfo -> cancelled) {
            errno = tinfo -> cancelled;
            return -1;
        }
    } else { // 添加事件失败
        if(timer) {
            timer -> cancel();
        }
        SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }

    int error = 0;
    socklen_t len = sizeof(int);
    // 获取套接字的错误状态
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    // 没有错误，连接成功
    if(!error) {
        return 0;
    // 有错误，连接失败
    } else {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    int fd = do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0) {
        sylar::FdMgr::GetInstance() -> get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    if(!sylar::t_hook_enable) {
        return close_f(fd);
    }

    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
    if(ctx) {
        auto iom = sylar::IOManager::GetThis();
        if(iom) {
            // 取消所有事件
            iom -> cancelAll(fd);
        }
        // 在文件管理中删除fd
        sylar::FdMgr::GetInstance() -> del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ ) {
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
                if(!ctx || ctx -> isClose() || !ctx -> isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx -> setUserNonblock(arg & O_NONBLOCK);
                if(ctx -> getSysNonblock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
                if(!ctx || ctx -> isClose() || !ctx -> isSocket()) {
                    return arg;
                }
                if(ctx -> getUserNonblock()) {
                    return arg | O_NONBLOCK;
                } else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

/*	value为指向int类型的指针，如果该指针指向的值为0，则表示关闭非阻塞模式；如果该指针指向的值为非0，则表示打开非阻塞模式。
 *	int value = 1;
 *	ioctl(fd, FIONBIO, &value);
 */
int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);
    //	FIONBIO用于设置文件描述符的非阻塞模式
    if(FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg;
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(d);
        if(!ctx || ctx -> isClose() || !ctx -> isSocket()) {
            return ioctl_f(d, request, arg);
        }
        ctx -> setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if(!sylar::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    // 如果设置socket通用选项
    if(level == SOL_SOCKET) {
        // 如果设置超时选项
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(sockfd);
            if(ctx) {
                const timeval* v = (const timeval*)optval;
                // 转为毫秒保存
                ctx -> setTimeout(optname, v -> tv_sec * 1000 + v -> tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}
