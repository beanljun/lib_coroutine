/**
 * @file timer.h
 * @brief 定时器模块
 * @details 定时器模块，提供定时器的管理，定时器的触发，定时器的取消等功能
 * @author beanljun
 * @date 2024-05-09
*/

#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__

#include <memory>
#include <set>
#include <vector>
#include "include/mutex.h"

namespace sylar {

// 定时器类
class Timer : public std::enable_shared_from_this<Timer> {  // 继承自std::enable_shared_from_this，用于获取当前对象的智能指针
friend class TimerManager;  // 声明定时器管理器为定时器的友元类
    typedef std::shared_ptr<Timer> ptr; // 定时器的智能指针
    /// @brief  取消定时器
    bool cancel();

    /// @brief  刷新定时器的执行时间
    bool refresh();

    /**
     * @brief 重置定时器时间
     * @param[in] ms 定时器执行间隔时间(毫秒)
     * @param[in] from_now 是否从当前时间开始计算
     */
    bool reset(uint64_t ms, bool from_now);

private:
    /**
     * @brief 构造函数
     * @param[in] ms 定时器执行间隔时间(毫秒)
     * @param[in] cb 回调函数
     * @param[in] recurring 是否循环
     * @param[in] manager 定时器管理器指针
    */
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);

    /**
     * @brief 构造函数
     * @param[in] next 执行的时间戳（毫秒）
    */
    Timer(uint64_t next);

private:
    bool m_recurring = false;           // 是否循环定时器
    uint64_t    m_ms = 0;               // 执行周期
    uint64_t  m_next = 0;               // 精确的执行时间
    std::function<void()> m_cb;         // 定时器回调函数
    TimerManager* m_manager = nullptr;  // 定时器管理器指针

private:
    // 定时器比较仿函数
    struct Comparator {
        /**
         * @brief 重载()操作符，用于定时器的比较、排序
         * @details 比较两个定时器的执行时间，按照执行时间从小到大排序，如果执行时间相同，地址小的排在前面
         * @param[in] lhs 定时器指针_left hand side
         * @param[in] rhs 定时器指针_right hand side
        */
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};

// 定时器管理器类
class TimerManager {
friend class Timer;  // 声明定时器为定时器管理器的友元类

public:
    typedef RWMutex RWMutexType;  // 读写锁类型

    /// @brief 构造函数
    TimerManager();
    /// @brief 析构函数，虚函数便于子类继承在析构时释放资源
    virtual ~TimerManager();

    /**
     * @brief 添加定时器
     * @param[in] ms 定时器执行间隔时间(毫秒)
     * @param[in] cb 定时器回调函数
     * @param[in] recurring 是否循环定时器
    */
    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);

    /**
     * @brief 添加条件定时器
     * @param[in] ms 定时器执行间隔时间(毫秒)
     * @param[in] cb 定时器回调函数
     * @param[in] weak_cond 条件
     * @param[in] recurring 是否循环定时器
    */
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb
                                ,std::weak_ptr<void> weak_cond
                                ,bool recurring = false);

    /// @brief 获取下一个定时器执行的时间
    uint64_t getNextTimer();

    /// @brief 获取需要执行的定时器的回调函数列表 
    /// @param cbs 回调函数列表
    void listExpiredCb(std::vector<std::function<void()>>& cbs);

    /// @brief 是否有定时器
    bool hasTimer();

protected:

    /**
     * @brief 当有新的定时器插入到定时器的首部,执行该函数，该函数主要用于触发条件变量
     * @details 纯虚函数，字类需要实现
     */
    virtual void onTimerInsertedAtFront() = 0;

    /// @brief 将定时器添加到管理器中
    void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);

private:

    /// @brief 检测服务器时间是否被调后了
    bool detectClockRollover(uint64_t now_ms);

private:

    RWMutexType m_mutex;       
    std::set<Timer::ptr, Timer::Comparator> m_timers;   // 定时器集合
    bool m_tickled = false;         // 是否触发onTimerInsertedAtFront
    uint64_t m_previousTime = 0;    // 上次执行时间
};

} // namespace sylar

#endif  // __SYLAR_TIMER_H__