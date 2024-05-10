/**
 * @file timer.cc
 * @brief 定时器模块实现
 * @details 定时器模块，提供定时器的管理，定时器的触发，定时器的取消等功能
 * @author beanljun
 * @date 2024-05-09
*/

#include "include/timer.h"
#include "include/util.h"
#include "include/macro.h"

namespace sylar {

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    if(lhs == rhs) return false;
    if(!lhs || lhs -> m_next < rhs -> m_next) return true;
    if(!rhs || rhs -> m_next < lhs -> m_next) return false;
    return lhs.get() < rhs.get(); // 如果执行时间相同，比较两个指针的地址
}

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager)
    :m_recurring(recurring)
    ,m_ms(ms)
    ,m_cb(cb)
    ,m_manager(manager) { m_next = sylar::GetCurrentMS() + m_ms; }

Timer::Timer(uint64_t next) : m_next(next) {}

bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
    if(m_cb) {
        m_cb = nullptr;
        auto it = m_manager -> m_timers.find(shared_from_this());   // 从定时器集合中查找当前定时器
        m_manager -> m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
    if(!m_cb) return false; // 如果回调函数为空，直接返回

    auto it = m_manager -> m_timers.find(shared_from_this());// step1: 从定时器集合中查找当前定时器
    if(it == m_manager -> m_timers.end()) return false; // 如果定时器不在定时器集合中，直接返回

    m_manager -> m_timers.erase(it);                 // step2: 从定时器集合中删除当前定时器
    m_next = sylar::GetCurrentMS() + m_ms;           // step3: 重新计算下次执行时间
    m_manager -> m_timers.insert(shared_from_this());// step4: 将当前定时器重新添加到定时器集合中
    return true;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    if(ms == m_ms && !from_now) return true; // 如果定时器执行周期和当前周期相同，并且不是从当前时间开始计算，已经是最新的定时器，直接返回true
    TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
    if(!m_cb) return false; // 如果回调函数为空，直接返回

    auto it = m_manager -> m_timers.find(shared_from_this()); // step1: 从定时器集合中查找当前定时器
    if(it == m_manager -> m_timers.end()) return false; // 如果定时器不在定时器集合中，直接返回

    m_manager -> m_timers.erase(it); // step2: 从定时器集合中删除当前定时器
    uint64_t start_t = 0;
    if(from_now) start_t = sylar::GetCurrentMS(); // step3: 如果是从当前时间开始计算，直接使用当前时间
    else start_t = m_next - m_ms;                 // 否则，重新计算执行时间
    m_ms = ms;                                    
    m_next = start_t + m_ms;                      // step4: 重置的时间为开始时间+执行周期
    m_manager -> m_timers.insert(shared_from_this()); // step5: 将当前定时器重新添加到定时器集合中
    return true;
}

TimerManager::TimerManager() {
    m_previousTime = sylar::GetCurrentMS();
}

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));   // 创建一个定时器，返回智能指针
    RWMutexType::WriteLock lock(m_mutex);
    m_timers.insert(timer);     // 将定时器添加到定时器集合中
    return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    std::shared_ptr<void> tmp = weak_cond.lock(); // 将条件变量转换为shared_ptr
    if(tmp) cb();                                 // 如果条件变量有效，执行回调函数
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring) {
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring); // 将条件变量转换为shared_ptr，绑定到回调函数中
}

uint64_t TimerManager::getNextTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    m_tickled = false;
    if(m_timers.empty()) return ~0ull; // 如果定时器集合为空，返回最大值， ~0ull表示无符号长整型最大值

    const Timer::ptr& next = *m_timers.begin(); // 获取定时器集合中最早执行的定时器
    uint64_t now_ms = sylar::GetCurrentMS();
    if(now_ms >= next -> m_next) return 0; // 如果当前时间大于等于下次执行时间，返回0
    return next -> m_next - now_ms; // 返回下次执行时间和当前时间的差值
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
    uint64_t now_ms = sylar::GetCurrentMS();
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock lock(m_mutex);
        if(m_timers.empty()) return;
    }

    RWMutexType::WriteLock lock(m_mutex);
    if(m_timers.empty()) return; 
    bool rollover = false; // 是否发生了时间回拨
    if(SYLAR_UNLIKELY(detectClockRollover(now_ms)))  rollover = true; // 检测是否发生了时间回拨

    if(!rollover && ((*m_timers.begin()) -> m_next > now_ms)) return; // 如果没有发生时间回拨，并且最早执行的定时器的执行时间大于当前时间，直接返回

    Timer::ptr now_timer(new Timer(now_ms)); // 以当前时间创建一个定时器
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer); // 如果发生了时间回拨，从定时器集合的末尾开始查找，否则从不小于当前时间的定时器开始查找
    while(it != m_timers.end() && (*it) -> m_next == now_ms) { // 遍历所有执行时间等于当前时间的定时器
        ++it;
    }
    expired.insert(expired.begin(), m_timers.begin(), it); // 将所有执行时间小于或等于当前时间的定时器添加到过期定时器集合中
    m_timers.erase(m_timers.begin(), it); // 从定时器集合中删除所有执行时间小于或等于当前时间的定时器
    cbs.reserve(expired.size());         // 预留空间
    for(auto& timer : expired) {
        cbs.push_back(timer -> m_cb); // 将过期定时器的回调函数添加到回调函数集合中
        if(timer -> m_recurring) { // 如果定时器是重复执行的
            timer -> m_next = now_ms + timer -> m_ms; // 重新计算下次执行时间
            m_timers.insert(timer); // 将定时器重新添加到定时器集合中
        } else timer -> m_cb(); // 执行定时器的回调函数
    }
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock) {
    auto it = m_timers.insert(val).first; // 将定时器添加到定时器集合中, .first表示返回一个pair，pair的first表示定时器的迭代器
    bool at_front = (it == m_timers.begin()) && !m_tickled; // 判断是否是最早执行的定时器
    if(at_front) m_tickled = true; // 如果是最早执行的定时器，设置m_tickled为true
    lock.unlock(); // 解锁
    if(at_front) onTimerInsertedAtFront(); // 判断两次是为了在多线程环境中避免竞态条件，确保在解锁后，如果`at_front`仍为`true`
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
    bool rollover = false;
    if(now_ms < m_previousTime && now_ms < (m_previousTime - 60 * 60 * 1000)) rollover = true; // 如果当前时间小于上一次时间，并且小于上一次时间减去1小时，认为发生了时间回拨
    m_previousTime = now_ms; // 更新上一次时间
    return rollover;
}

bool TimerManager::hasTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}

} // namespace sylar