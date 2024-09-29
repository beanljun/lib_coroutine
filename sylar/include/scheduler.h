/**
 * @file scheduler.h
 * @author beanljun
 * @brief 协程调度器
 * @date 2024-05-06
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <memory>
#include <list>
#include <string>
#include <functional>

#include "log.h"
#include "fiber.h"
#include "thread.h"

namespace sylar {

/**
 * @brief 协程调度器
 * @details 封装N-M的协程调度器
 *          内部有一个线程池，支持协程在线程池里面切换
 */

class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    /**
     * @brief 调度器构造函数
     * @param[in] threads 线程数量， 默认1
     * @param[in] use_caller 是否将当前线程也作为调度线程， 默认true
     * @param[in] name 协程调度器名称， 默认"Scheduler"
     */
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "Scheduler");

    virtual ~Scheduler();//析构函数, 虚函数为了子类能够继承，子类可以顺利释放资源

    /// 获取调度器名称
    const std::string &getName() const { return m_name; } 

    /// 获取当前线程调度器指针
    static Scheduler *GetThis(); 

    /// 获取当前线程的主协程
    static Fiber *GetMainFiber(); 

    template<class FiberOrCb>
    void schedule(FiberOrCb fc, int thread = -1) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(fc, thread);
        }

        if(need_tickle) {
            tickle(); // 唤醒线程
        }
    }

    /// 启动协程调度器  创建指定数量的线程，并开始运行
    void start(); 

    /// 停止协程调度器，所有调度任务都执行完了再返回
    void stop(); 

protected:
    /**
     * @brief 通知协程调度器有任务了
     */
    virtual void tickle(); 

    /// 协程调度函数
    void run(); 

    /// 无任务时执行idle协程
    virtual void idle(); 

    /// 是否停止
    /// 检查调度器是否已经完全停止，包括所有的任务都已经处理完毕，所有的线程都已经停止。
    virtual bool stopping(); 

    /// 设置当前协程的调度器
    void setThis();

    /**
     * @brief 返回是否有空闲线程
     * @details 当调度协程进入idle时空闲线程数加1，从idle协程返回时空闲线程数减1
     */
    bool hasIdleThreads() { return m_idleThreadCount > 0;} 

private:

    /**
     * @brief 添加调度任务
     * @tparam FiberOrCb 调度任务类型， 协程对象或者是函数指针
     * @param fc 协程对象或者函数指针
     * @param thread 指定运行该任务的线程号， 默认为-1，表示任意线程
     */
    template <class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread) {
        bool need_tickle = m_tasks.empty();
        ScheduleTask task(fc, thread);

        if(task.fiber || task.cb) m_tasks.emplace_back(task);
        return need_tickle;
    }

private:
    /**
     * @brief 调度任务，协程/函数二选一，可指定在哪个线程上调度
     */
    struct ScheduleTask {
        Fiber::ptr fiber;
        std::function<void()> cb;
        int thread;

        ScheduleTask(Fiber::ptr f, int thr) {
            fiber   = f;
            thread  = thr;
        }

        ScheduleTask(Fiber::ptr *f, int thr) {
            fiber.swap(*f);
            thread = thr;
        }

        ScheduleTask(std::function<void()> f, int thr) {
            cb      = f;
            thread  = thr;
        }

        ScheduleTask() { thread = -1; }

        void reset() {
            fiber   = nullptr;
            cb      = nullptr;
            thread  = -1;
        }
    };

private:

    std::string m_name;                                 /// 调度器名称
    MutexType m_mutex;                                  /// 互斥锁
    std::vector<Thread::ptr> m_threads;                 /// 线程池
    std::list<ScheduleTask> m_tasks;                    /// 任务队列
    std::vector<int> m_threadIds;                       /// 线程池线程id数组
    size_t m_threadCount = 0;                           /// 工作线程数量，不包括use_caller主线程
    std::atomic<size_t> m_activeThreadCount = {0};      /// 活跃的线程数量
    std::atomic<size_t> m_idleThreadCount = {0};        /// idle线程数量

    bool m_useCaller;                                   /// 是否use caller
    int m_rootThread = 0;                               /// use_caller为true时，调度器所在线程的id
    
    Fiber::ptr m_rootFiber;                             /// use_caller为true时，调度器所在线程的调度协程
    bool m_stopping = false;                             /// 是否正在停止                           

};


} // namespace sylar






#endif // __SYLAR_SCHEDULER_H__