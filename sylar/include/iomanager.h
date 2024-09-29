/**
 * @file iomanager.h
 * @brief IO协程调度器
 * @author beanljun
 * @date 2024-5-08
*/

#ifndef __IOMANAGER_H__
#define __IOMANAGER_H__

#include "timer.h"
#include "scheduler.h"

namespace sylar {
    
class IOManager : public Scheduler, public TimerManager {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    /// @brief IO事件，继承自epoll_event对事件的定义
    /// @details 这里只关心socket fd的读和写事件，其他epoll事件会归类到这两类事件中
    enum Event {
        NONE  = 0x0,    // 无事件
        READ  = 0x1,    // 读事件(EPOLLIN)
        WRITE = 0x4     // 写事件(EPOLLOUT)
    };

private:
    /// @brief Socket事件上下文
    /// @details 每个socket fd都对应一个FdContext，包括fd的值，fd上的事件，以及fd的读写事件上下文
    struct FdContext {
        typedef Mutex MutexType;
        /**
         * @brief 事件上下文类
         * @details fd的每个事件都有一个事件上下文，保存这个事件的回调函数以及执行回调函数的调度器
         *          sylar对fd事件做了简化，只预留了读事件和写事件，所有的事件都被归类到这两类事件中
         */ 
        struct EventContext {
            Scheduler* scheduler = nullptr;  /// 执行事件回调的调度器
            Fiber::ptr fiber;                /// 事件回调协程
            std::function<void()> cb;        /// 事件回调函数
        };

        /// 获取事件上下文
        EventContext& getEventContext(Event event);
        /// 重置事件上下文
        void resetEventContext(EventContext& ctx);
        /// 触发事件, 根据事件类型调用对应上下文结构中的调度器去调度回调协程或回调函数
        void triggerEvent(Event event);

        EventContext read;      /// 读事件上下文
        EventContext write;     /// 写事件上下文
        int fd = 0;             /// 事件关联的fd
        Event events = NONE;    ///该fd添加了哪些事件的回调函数，或者说该fd关心哪些事件
        MutexType mutex;        /// 事件上下文的锁
    };

public:
    /**
     * @brief 构造函数
     * @param[in] threads IO协程调度器的线程数量
     * @param[in] use_caller 是否使用当前调用线程
     * @param[in] name IO协程调度器的名称
    */
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "IOManager");

    /// @brief 析构函数
    ~IOManager();

    /**
     * @brief 添加事件
     * @details fd描述符发生了event事件时执行cb函数
     * @param[in] fd socket句柄
     * @param[in] event 事件类型
     * @param[in] cb 事件回调函数，如果为空，则默认把当前协程作为回调执行体
     * @return 添加成功返回0,失败返回-1
     */
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);

    /**
     * @brief 删除事件
     * @param[in] fd socket句柄
     * @param[in] event 事件类型
     * @return 是否删除成功
     */
    bool delEvent(int fd, Event event);

    /**
     * @brief 取消事件
     * @param[in] fd socket句柄
     * @param[in] event 事件类型
     * @attention 如果该事件被注册过回调，那就触发一次回调事件
     * @return 是否取消成功
     */
    bool cancelEvent(int fd, Event event);
    /**
     * @brief 取消所有事件
     * @details 所有被注册的回调事件在cancel之前都会被执行一次
     * @param[in] fd socket句柄
     * @return 是否取消成功
     */
    bool cancelAll(int fd);

    /**
     * @brief 返回当前的IOManager
     */
    static IOManager* GetThis();

protected:

    /// @brief 通知调度器有任务要调度， 写pipe让idle协程从epoll_wait退出，待idle协程yield之后Scheduler::run就可以调度其他任务
    void tickle() override;

    /// @brief 判断是否可以停止， 判断条件是Scheduler::stopping()外加IOManager的m_pendingEventCount为0，表示没有IO事件可调度了
    bool stopping() override;

    /// @brief idle协程， 对于IO协程调度来说，应阻塞在等待IO事件上，idle退出的时机是epoll_wait返回，对应的操作是tickle或注册的IO事件发生
    void idle() override;

    /**
     * @brief 判断是否可以停止，同时获取最近一个定时器的超时时间
     * @param[out] timeout 最近一个定时器的超时时间，用于idle协程的epoll_wait
     * @return 返回是否可以停止
     */
    bool stopping(uint64_t& timeout);

    /// @brief 当有定时器插入到头部时，要重新更新epoll_wait的超时时间，这里是唤醒idle协程以便于使用新的超时时间
    void onTimerInsertedAtFront() override;

    /// @brief 重置socket句柄上下文的容器大小
    void contextResize(size_t size);

private:
    int m_epfd = 0;                                 /// epoll文件句柄
    int m_tickleFds[2];                             /// pipe 文件句柄，fd[0]读端，fd[1]写端
    std::atomic<size_t> m_pendingEventCount = {0};  /// 当前待处理的事件数量
    RWMutexType m_mutex;                            /// 读写锁
    std::vector<FdContext*> m_fdContexts;           /// fd上下文数组
};

} // namespace sylar

#endif