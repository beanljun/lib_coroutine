/**
 * @file util.h
 * @author beanljun
 * @brief 
 * @date 2024-04-25
 */

#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include <stdint.h>
#include <sys/types.h>
#include <string>

namespace sylar {

/**
 * @brief 获取当前线程ID
 * @note 注意pid_t和pthread_t的区别
 */
pid_t GetThreadId();

/**
 * @brief 获取当前协程ID
 * @todo 暂未实现，返回0
 */

uint64_t GetFiberId();

/**
 * @brief 获取当前时间的毫秒
 */

uint64_t GetElapsed();

/**
 * @brief 获取线程的名称
 */
std::string GetThreadName();

/**
 * @brief 设置线程的名称
 */

void SetThreadName(const std::string& name);

}

#endif