/**
 * @file mutex.h
 * @author beanljun
 * @brief 信号量，互斥锁，读写锁等的封装
 * thread是基于pthread实现的,并且C++11里面没有提供读写互斥量，RWMutex，Spinlock等，
 * 在高并发场景，这些对象是经常需要用到的，所以选择了封装pthread
 * @date 2024-04-24
 */

#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <stdexcept>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>

#include "include/noncopyable.h"

namespace sylar {

/**
 * @brief 信号量封装
 */
class Semaphore : Noncopyable {
public:

    Semaphore(uint32_t count = 0);//构造函数, count为信号量的大小，默认为0

    ~Semaphore();

    void wait(); //获取信号量

    void notify(); //释放信号量

private:
    sem_t m_semaphore; //信号量
};

/**
 * @brief 局部锁模板类
 * struct的作用是为了让模板类的成员变量和成员函数都是public的，
 * 而class的成员变量和成员函数默认是private的
 */
template<class T>
struct ScopedLockImpl {
public:
    ScopedLockImpl(T& mutex) : m_mutex(mutex) {
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl() {
        unlock();
    }

    void  lock() {
        if(!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

/**
 * @brief 局部读锁模板类
 */
template<class T>
struct ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T& mutex) : m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }

    ~ReadScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

/**
 * @brief 局部写锁模板类
 */
template<class T>
struct WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T& mutex) : m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }

    ~WriteScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

/**
 * @brief 互斥锁
 */
class Mutex : Noncopyable {
public:
    typedef ScopedLockImpl<Mutex> Lock; //定义局部锁

    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex;
};

/**
 * @brief 读写互斥锁
 */
class RWMutex : Noncopyable {
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock; //定义局部读锁
    typedef WriteScopedLockImpl<RWMutex> WriteLock; //定义局部写锁

    RWMutex() {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&m_lock);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }

private:
    pthread_rwlock_t m_lock;
};

/**
 * @brief 自旋锁
 */
class Spinlock : Noncopyable {
public:
    typedef ScopedLockImpl<Spinlock> Lock; //定义局部锁

    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }

    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    void lock() {
        pthread_spin_lock(&m_mutex);
    }

    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }

private:
    pthread_spinlock_t m_mutex;
};

/**
 * @brief 原子锁
 */
class CASLock : Noncopyable {
public:
    typedef ScopedLockImpl<CASLock> Lock; //定义局部锁

    CASLock() {
        m_mutex.clear();
    }

    ~CASLock() {
    }

    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }

private:
    volatile std::atomic_flag m_mutex;
};

}

#endif