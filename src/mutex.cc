/**
 * @file mutex.cc
 * @author beanljun
 * @brief 信号量实现
 * @date 2024-04-24
 */

#include <stdexcept>
#include "include/mutex.h"

namespace sylar {

    Semaphore::Semaphore(uint32_t count) {
        if(sem_init(&m_semaphore, 0, count)) {
            throw std::logic_error("sem_init error"); //初始化信号量失败,抛出异常
        }
    }

    Semaphore::~Semaphore() {
        sem_destroy(&m_semaphore); //销毁信号量
    }
    /**
     * @brief 减少信号量的计数
     * `sem_wait`是一个POSIX信号量函数，用于减少信号量的值。
     * 如果信号量的值大于0，`sem_wait`会立即返回并将信号量的值减1。
     * 如果信号量的值为0，`sem_wait`会阻塞，直到信号量的值大于0。
     * 当一个线程需要等待其他线程完成任务时，它可以调用`wait`方法来等待。
     */
    void Semaphore::wait() {
        if(sem_wait(&m_semaphore)) {
            throw std::logic_error("sem_wait error"); //获取信号量失败,抛出异常
        }
    }

    /**
     * @brief 增加信号量的计数
     * `sem_post`是一个POSIX信号量函数，用于增加信号量的值。
     * 如果成功，`sem_post`返回0，否则返回-1。
     * 当一个线程完成了一项任务，它可以调用`notify`方法来通知其他可能正在等待的线程。
     */
    void Semaphore::notify() {
        if(sem_post(&m_semaphore)) {
            throw std::logic_error("sem_post error"); //释放信号量失败,抛出异常
        }
    }

}
