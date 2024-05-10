/**
 * @file fiber.cc
 * @brief 轻量级协程封装
 * @author beanljun
 * @date 2024-04-20
*/
#include <atomic>

#include <include/macro.h>
#include "include/fiber.h"
#include "include/log.h"
#include "include/config.h"
#include "include/scheduler.h"

namespace sylar {

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

/// 用于生成协程id
static std::atomic<uint64_t> s_fiber_id {0};
/// 用于统计协程数量
static std::atomic<uint64_t> s_fiber_count {0};

/// 线程局部变量，当前正在运⾏的协程
static thread_local Fiber* t_fiber = nullptr;
/// 线程局部变量，当前线程的主协程，智能指针形式
static thread_local Fiber::ptr t_thread_fiber = nullptr;

//协程栈大小，默认128KB，可以通过配置修改
static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");

/**
 * @brief malloc栈内存分配器
 * @details 用于分配协程的栈内存，alloc⽅法⽤于分配内存，dealloc⽅法⽤于释放内存
 */
class MallocStackAllocator {
public:
    static void *Alloc(size_t size) { return malloc(size); }
    static void Dealloc(void *vp, size_t size) { return free(vp); }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId() {
    if (t_fiber) {
        return t_fiber -> getId();
    }
    return 0;
}

Fiber::Fiber() {
    SetThis(this); // 设置当前协程
    m_state = RUNNING;

    if (getcontext( &m_ctx)) {
        SYLAR_ASSERT2(false, "getcontext");
    }
    ++s_fiber_count;
    m_id = s_fiber_id++; // 协程id从0开始，用完加1

    SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber() main id = " << m_id;
}

void Fiber::SetThis(Fiber* f) {
     t_fiber = f;
}

Fiber::ptr Fiber::GetThis() {
    
    if (t_fiber) {
        return t_fiber -> shared_from_this();
    }

    ///如果当前线程还未创建协程，则创建线程的第一个协程
    Fiber::ptr main_fiber(new Fiber);
    SYLAR_ASSERT(t_fiber == main_fiber.get());
    t_thread_fiber = main_fiber;
    return t_fiber -> shared_from_this();
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler)
    : m_id(s_fiber_id++)
    , m_cb(cb)
    , m_runInScheduler(run_in_scheduler){

    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size -> getValue();
    m_stack = StackAllocator::Alloc(m_stacksize);

    if(getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);

    SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber() id = " << m_id;

}

Fiber::~Fiber() {
    SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber() id = " << m_id;
    --s_fiber_count;
    // 根据栈内存是否为空，进行不同的释放操作
    if (m_stack) {
        // 有栈，为子协程， 需确保子协程为结束状态
        SYLAR_ASSERT(m_state == TERM);
        StackAllocator::Dealloc(m_stack, m_stacksize);
        SYLAR_LOG_DEBUG(g_logger) << "Dealloc Stack, id = " << m_id;
    } else {
        // 无栈，为主协程, 主协程没有入口函数，运行状态一定为RUNNING
        SYLAR_ASSERT(!m_cb);
        SYLAR_ASSERT(m_state == RUNNING);

        Fiber* cur = t_fiber;
        if (cur == this) {
            SetThis(nullptr);
        }
    }
}

void Fiber::reset(std::function<void()> cb) {
    SYLAR_ASSERT(m_stack);
    SYLAR_ASSERT(m_state == TERM);
    m_cb = cb;
    if (getcontext(&m_ctx)) SYLAR_ASSERT2(false, "getcontext");

    m_ctx.uc_link          = nullptr;
    m_ctx.uc_stack.ss_sp   = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = READY;
}

void Fiber::resume() {
    SYLAR_ASSERT(m_state != TERM && m_state != RUNNING);
    SetThis(this);
    m_state = RUNNING;

    // 如果协程参与调度器调度，应该和调度器的主协程进行swap，而不是和线程的主协程进行swap，yeld同理
    if (m_runInScheduler) {
        if(swapcontext(&(Scheduler::GetMainFiber() -> m_ctx), &m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    } else {
        if (swapcontext(&t_thread_fiber -> m_ctx, &m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }


}

void Fiber::yield() {

    SYLAR_ASSERT(m_state == RUNNING || m_state == TERM);
    SetThis(t_thread_fiber.get());
    if (m_state != TERM) {
        m_state = READY;
    }

    // 如果协程参与调度器调度，那么应该和调度器的主协程进行swap，而不是线程主协程
    if (m_runInScheduler) {
        if (swapcontext(&m_ctx, &(Scheduler::GetMainFiber() -> m_ctx))) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    } else {
        if (swapcontext(&m_ctx, &(t_thread_fiber -> m_ctx))) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }
}

void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis(); //GetThis()的shared_from_this()⽅法让引用计数+1
    SYLAR_ASSERT(cur);
    
    cur -> m_cb();//这里真正执行协程的入口函数
    cur -> m_cb = nullptr;
    cur -> m_state = TERM;

    auto raw_ptr = cur.get();//手动让t_fiberde的引用计数-1
    cur.reset();
    raw_ptr -> yield();// 协程结束时⾃动yield, 切换到主协程
}


}
