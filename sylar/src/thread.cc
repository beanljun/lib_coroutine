/**
 * @file thread.cc
 * @author beanljun
 * @brief 线程封装
 * @date 2024-04-24
 */

#include "../include/thread.h"

#include "../include/log.h"
#include "../util/util.h"

namespace sylar {

static thread_local Thread *    t_thread = nullptr;        // 线程局部变量-当前线程指针，默认为空
static thread_local std::string t_thread_name = "unknow";  // 线程局部变量-当前线程名称，默认为unknow

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

Thread::Thread(std::function<void()> cb, const std::string &name) : m_cb(cb), m_name(name) {
    if (name.empty())
        m_name = "unknow";
    int res = pthread_create(&m_thread, nullptr, &Thread::run, this);  // 创建线程, 成功返回0
    if (res) {
        SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, res = " << res << " name = " << name;
        throw std::logic_error("pthread_create error");  //创建线程失败，抛出异常
    }
    m_semaphore.wait();  // 等待线程执行
}

Thread::~Thread() {
    if (m_thread) {
        pthread_detach(m_thread);  // 分离线程
    }
}

Thread *Thread::GetThis() {
    return t_thread;
}

const std::string &Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string &name) {
    if (name.empty())
        return;  // 如果name为空，则直接返回
    if (t_thread)
        t_thread->m_name = name;  // 如果当前线程指针不为空，则设置线程名
    t_thread_name = name;         // 设置线程局部变量-当前线程名称
}

void Thread::join() {
    if (m_thread) {
        int res = pthread_join(m_thread, nullptr);  // 等待线程执行完成，成功返回0
        if (res) {
            SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail, res = " << res << " name = " << m_name;
            throw std::logic_error("pthread_join error");  //线程执行失败，抛出异常
        }
        m_thread = 0;  //线程执行完成，将线程结构置为0
    }
}

void *Thread::run(void *arg) {
    Thread *thread = (Thread *)arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = sylar::GetThreadId();
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());  //设置线程名称

    std::function<void()> cb;
    cb.swap(thread->m_cb);  //交换线程执行函数

    thread->m_semaphore.notify();

    cb();  // 执行线程函数
    return 0;
}

}  // namespace sylar
