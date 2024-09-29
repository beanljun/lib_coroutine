/**
 * @file thread.h
 * @author beanljun
 * @brief 线程封装
 * @date 2024-04-24
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#include <string>

#include "mutex.h"

namespace sylar {

/**
 * @brief 线程类
 */
class Thread : Noncopyable {
public:
    typedef std::shared_ptr<Thread> ptr; // 类型别名，用ptr表示线程智能指针

    Thread(std::function<void()> cb, const std::string &name); // 构造函数

    ~Thread(); // 析构函数


    //成员函数后面的`const`关键字表示该成员函数是一个常量成员函数，它不能修改类的任何数据成员。

    /// 获取线程id, 返回pid_t类型，用于表示线程id
    pid_t getId() const { return m_id; } 

    const std::string &getName() const { return m_name; } // 获取线程名

    /// 等待线程执行完成
    void join(); 

    //在成员函数前面使用`static`关键字，表示该函数是一个静态成员函数。
    //静态成员函数不依赖于类的任何对象，可以直接通过类名来调用
    /// 获取当前线程指针
    static Thread *GetThis(); 

    /// 获取当前线程名称
    static const std::string &GetName(); 
    
    /// 设置当前线程名称
    static void SetName(const std::string &name); 

private:
    /// 线程执行函数
    static void *run(void *arg); 

private:
    pid_t m_id = -1;                // 线程id, 默认为-1, 表示无效
    pthread_t m_thread = 0;         // 线程结构体
    std::function<void()> m_cb;     // 线程执行函数
    std::string m_name;             // 线程名称
    Semaphore m_semaphore;          // 信号量
};


}

#endif // __SYLAR_THREAD_H__