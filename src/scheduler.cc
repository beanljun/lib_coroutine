/**
 * @file scheduler.cc
 * @author beanljun
 * @brief 一个简单的协程调度器
 * @date 2024-04-24
 */

#include "include/scheduler.h"
#include "include/macro.h"
#include "include/hook.h"

 namespace sylar {
    
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    /// 实例化一个当前线程的调度器，同一个调度器下的所有线程共享一个调度器
    static thread_local Scheduler *t_scheduler = nullptr; 
    /// 当前线程的调度协程，每个线程（包括caller线程）有一个调度协程，
    static thread_local Fiber *t_scheduler_fiber = nullptr;


    // step 1 设置`m_useCaller`和`m_name`成员变量的值。  
    //  
    // step 2 `use_caller`为`true`，则将调用者线程作为工作线程之一。
    //        减少一个新创建的工作线程的数量（`--threads;`），并获取当前线程的主协程。
    //        创建一个新的协程`m_rootFiber`，并将其设置为调度协程。
    //        将调用者线程的ID添加到`m_threadIds`列表中。  
    //  
    //  step 3 如果`use_caller`为`false`，则将`m_rootThread`设置为-1，表示没有使用调用者线程。 
    //  
    //  step 4 设置`m_threadCount`为`threads`，表示新创建的工作线程的数量。  
    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name) {
        SYLAR_ASSERT(threads > 0);

        m_useCaller = use_caller;
        m_name = name;

        if(use_caller) {
            --threads;
            sylar::Fiber::GetThis(); // 获取当前线程的主协程
            SYLAR_ASSERT(GetThis() == nullptr);
            t_scheduler = this;

            /**
             * 在user_caller为true的情况下，初始化caller线程的调度协程
             * caller线程的调度协程不会被调度器调度，⽽且，caller线程的调度协程停⽌时，应该返回caller线程的主协程
             *
             * 
             * std::bind可以生成一个新的可调用对象，该对象绑定了一些参数。
             * 
             * std::bind(&Scheduler::run, this)生成了一个新的可调用对象，该对象绑定了成员函数`Scheduler::run`和对象`this`。
             * 当调用这个新生成的可调用对象时，会调用`Scheduler::run`函数，并自动传入`this`作为第一个参数。
             * 相当于调用了`this->run()`。
            */
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false));

            sylar::Thread::SetName(m_name);
            t_scheduler_fiber = m_rootFiber.get();
            m_rootThread = sylar::GetThreadId();
            m_threadIds.push_back(m_rootThread);
        } else m_rootThread = -1;

        m_threadCount = threads;
    }

    Scheduler *Scheduler::GetThis() { return t_scheduler; }

    Fiber *Scheduler::GetMainFiber() { return t_scheduler_fiber; }

    void Scheduler::setThis() { t_scheduler = this; }

    Scheduler::~Scheduler() {
        SYLAR_LOG_DEBUG(g_logger) << "Scheduler::~Scheduler()";
        SYLAR_ASSERT(m_stopping);
        if(GetThis() == this) t_scheduler = nullptr;
    }

    



    // step 1 检查`m_stopping`成员变量，如果它为`true`，则说明调度器正在停止，此时直接返回。
    // step 2 确保`m_threads`向量是空的，也就是说，确保调度器还没有启动，调整`m_threads`向量的大小为`m_threadCount`，也就是要创建的线程数量。
    // step 3 创建线程，线程对象被存储在`m_threads`向量中，线程的ID被添加到`m_threadIds`向量中。
    void Scheduler::start() {
        SYLAR_LOG_DEBUG(g_logger) << "start";
        MutexType::Lock lock(m_mutex);
        if(m_stopping) {
            SYLAR_LOG_ERROR(g_logger) << "start stopping return";
            return;
        }
        SYLAR_ASSERT(m_threads.empty());
        m_threads.resize(m_threadCount);
        for (size_t i = 0; i < m_threadCount; i++) {
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this),
                                        m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
            }
    }

    bool Scheduler::stopping() {
        MutexType::Lock lock(m_mutex);
        return m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
    }

    void Scheduler::tickle() {
        SYLAR_LOG_DEBUG(g_logger) << "tickle";
    }

    void Scheduler::idle() {
        SYLAR_LOG_DEBUG(g_logger) << "idle";
        while (!stopping()) {
            // 让出当前协程的执行权，切换到其他协程执行。 
            // 即使调度器处于空闲状态，也不会浪费CPU资源，而是让出CPU给其他协程使用。
            sylar::Fiber::GetThis() -> yield(); 
        }
    }

    void Scheduler::stop() {
        SYLAR_LOG_DEBUG(g_logger) << "stop";
        if(stopping()) return;
        // 进入stop将m_stopping设为true
        m_stopping = true;

        // 如果use caller，那只能由caller线程发起stop
        if(m_useCaller) {
            SYLAR_ASSERT(GetThis() == this);
        } else SYLAR_ASSERT(GetThis() != this);

        // 每个线程都tickle一下
        for(size_t i = 0; i < m_threadCount; i++)  tickle();

        // 使用use_caller多tickle一下
        if(m_rootFiber) tickle();

        // 在use caller情况下，调度器协程结束时，应该返回caller协程
        if(m_rootFiber) {
            m_rootFiber -> resume();
            SYLAR_LOG_DEBUG(g_logger) << "m_rootFiber end";
        }

        std::vector<Thread::ptr> thrs;
        {
            MutexType::Lock lock(m_mutex);
            thrs.swap(m_threads);
        }
        // 等待线程执行完成
        for(auto& i : thrs) {
            i -> join();
        }
    }

    void Scheduler::run() {
        SYLAR_LOG_DEBUG(g_logger) << "run";
        set_hook_enable(true);
        setThis();
        if(sylar::GetThreadId() != m_rootThread) t_scheduler_fiber = sylar::Fiber::GetThis().get();

        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));  // 创建一个idle协程，使用bind()将Scheduler::idle()函数绑定到idle协程上
        Fiber::ptr cb_fiber;                                                  // 回调协程
        ScheduleTask task;                                                    // 调度任务

        while(true) {
            task.reset();
            bool tickle_me = false;         // 是否唤醒其他线程进行任务调度
            {
                MutexType::Lock lock(m_mutex);
                auto it = m_tasks.begin();
                // 遍历任务队列，查找是否有任务需要执行
                while(it != m_tasks.end()) {
                    if(it -> thread != -1 && it -> thread != sylar::GetThreadId()) {
                        // 如果任务指定了调度线程，并且不是当前线程，标记唤醒其他线程，继续遍历
                        ++it;
                        tickle_me = true;
                        continue;
                    }

                    SYLAR_ASSERT(it -> fiber || it -> cb);//找到一个未指定线程 或者 指定线程为当前线程的任务

                    // // 任务队列时的协程一定是READY状态，谁会把RUNNING或TERM状态的协程加入调度呢？
                    // if(it -> fiber) SYLAR_ASSERT(it -> fiber -> getState() == Fiber::READY);
                    // [BUG FIX]: hook IO相关的系统调用时，在检测到IO未就绪的情况下，会先添加对应的读写事件，再yield当前协程，等IO就绪后再resume当前协程
                    // 多线程高并发情境下，有可能发生刚添加事件就被触发的情况，如果此时当前协程还未来得及yield，则这里就有可能出现协程状态仍为RUNNING的情况
                    // 这里简单地跳过这种情况，以损失一点性能为代价，否则整个协程框架都要大改
                    if(it -> fiber && it -> fiber -> getState() == Fiber::RUNNING) {
                        ++it;
                        continue;
                    }
                    // 当前调度线程找到一个任务，准备开始调度，将其从任务队列中剔除，活动线程数加1
                    task = *it;
                    m_tasks.erase(it++);
                    ++m_activeThreadCount;
                    break;
                }
                // 当前线程拿完一个任务后，发现任务队列还有剩余，那么tickle一下其他线程，让他们继续执行
                tickle_me |= (it != m_tasks.end());
            }

            if(tickle_me) tickle();

            if(task.fiber) { 
                // resume协程，resume返回时，协程要么执行完了，要么半路yield了，总之这个任务就算完成了，活跃线程数减一
                task.fiber -> resume();  // 恢复协程的执行
                --m_activeThreadCount;   // 活动线程数减一
                task.reset();        
            } else if(task.cb) {  // 如果任务是一个回调函数
                if(cb_fiber) cb_fiber -> reset(task.cb);  // 重置 cb_fiber 并设置其回调函数为 task.cb
                else cb_fiber.reset(new Fiber(task.cb));  // 创建一个新的 Fiber 对象，其回调函数为 task.cb
                task.reset(); 
                cb_fiber -> resume();   // 恢复 cb_fiber 的执行
                --m_activeThreadCount;  // 活动线程数减一
                cb_fiber.reset();       // 重置 cb_fiber
            } else {  
                // 进到这个分支情况一定是任务队列空了，调度idle协程即可
                if(idle_fiber -> getState() == Fiber::TERM) {  
                    // 如果调度器没有调度任务，那么idle协程会不停地resume/yield，不会结束，如果idle协程结束了，那一定是调度器停止了
                    SYLAR_LOG_DEBUG(g_logger) << "idle fiber term"; 
                    break;
                }
                ++m_idleThreadCount;
                idle_fiber -> resume(); 
                --m_idleThreadCount;
            }
        }
        SYLAR_LOG_DEBUG(g_logger) << "Scheduler::run() end";
    }



 }  // namespace sylar