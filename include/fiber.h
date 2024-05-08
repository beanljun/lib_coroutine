/**
 * @file "fiber.h"
 * @brief 轻量级协程封装
 * @author beanljun
 * @date 2024-04-20
*/
#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__

#include <memory>
#include <functional>
#include <ucontext.h>

#include "include/thread.h"

namespace sylar {

class Fiber : public std::enable_shared_from_this<Fiber> {

public:

    typedef std::shared_ptr<Fiber> ptr;

    /**
     * @brief 协程状态
     * 在sylar基础上进行了状态简化，由6个状态简化为3个状态：正在运行（RUNNING）、运行结束（TERM）、准备运行（READY）
     * 不区分初始状态，初始即为READY，不区分协程是异常结束还是正常结束，结束即为TERM
     * 也不区分HOLD状态（暂停），协程只要未结束也不在运行即为READY
     */
    enum State {
        /// 就绪态，刚创建或者yield之后的状态
        READY,
        /// 运行态，resume之后的状态 
        RUNNING,
        /// 结束态，协程的回调函数执⾏完之后为TERM状态
        TERM
    };

private:

    /**
     * @brief 构造函数
     * ⽆参构造函数只⽤于创建线程的第⼀个协程，也就是线程主函数对应的协程，
     * 这个协程只能由GetThis()⽅法调⽤，所以定义成私有⽅法
     */
    Fiber();

public:

    /**
     * @brief 构造函数，创建用户协程
     * @param[in] cb 协程入口函数
     * @param[in] stacksize 协程的栈⼤⼩
     * @param[in] run_in_scheduler 是否在调度器中运⾏,默认为true
     */
    Fiber(std::function<void()> cb, size_t stacksize = 0, bool run_in_scheduler = true);

    /**
     * @brief 析构函数
     */
    ~Fiber();

    /**
     * @brief 重置协程状态和入口函数，复用栈空间，不重新创建栈
     * @param[in] cb 协程入口函数
     */
    void reset(std::function<void()> cb);


    /**
     * @brief 将当前协程切换到运⾏状态
     * 当前协程和正在运行的协程进行交换，前者状态变为RUNNING，后者状态变为READY
     * 1. resume⼀个协程时，如果如果这个协程的m_runInScheduler值为true，
     * 表示这个协程参与调度器调度，那它应该和三个线程局部变量中的调度协程上下⽂进⾏切换，
     * 同理，在协程yield时，也应该恢复调度协程的上下⽂，表示从⼦协程切换回调度协程；
     * 2. 如果协程的m_runInScheduler值为false，表示这个协程不参与调度器调度，
     * 那么在resume协程时，直接和线程主协程切换就可以了，yield也⼀样，恢复线程主协程的上下⽂。
     * m_runInScheduler值为false的协程上下⽂切换完全和调度协程⽆关，可以脱离调度器使⽤。
     */
    void resume();

    /**
     * @brief 当前协程让出执行权
     * 当前协程与上次resume时退到后台的协程进行交换 前者状态变为READY，后者状态变为RUNNING
     */
    void yield();

    /**
     * @brief 获取协程id
     * @return 协程id
     */
    uint64_t getId() const { return m_id;}

    /**
     * @brief 获取协程状态
     * @return 协程状态
     */
    State getState() const { return m_state;}

public:

    /**
     * @brief 设置当前正在运行的协程，即设置线程局部变量t_fiber的值
    */
   static void SetThis(Fiber* f);

    /**
     * @brief 返回当前线程正在执行的协程
     * 如果当前线程还未创建协程，则创建线程的第一个协程，且该协程为当前线程的主协程，其他协程都通过这个协程来调度
     * 也就是说，其他协程结束时，都要切回到主协程，由主协程重新选择新的协程进行resume
     * @attention 线程如果要创建协程，那么首先应该执行一下Fiber::GetThis()，以初始化主函数协程
     */
    static Fiber::ptr GetThis();

    /**
     * @brief 获取总协程数
     */
    static uint64_t TotalFibers();

    /**
     * @brief 协程入口函数
     */
    static void MainFunc();

    /**
     * @brief 获取当前协程的id
     */
    static uint64_t GetFiberId();

private:

    /// id
    uint64_t m_id = 0;
    /// 栈大小
    uint32_t m_stacksize = 0;
    /// 状态
    State m_state = READY;
    /// 上下⽂
    ucontext_t m_ctx;
    /// 栈地址
    void *m_stack = nullptr;
    /// 入口函数
    std::function<void()> m_cb;
    /// 是否参与调度
    bool m_runInScheduler;


};
    
}

#endif // _FIBER_H_