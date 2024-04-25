/**
 * @file macro.h
 * @author beanljun
 * @brief 常用宏定义
 * @date 2024-04-25
 */

#ifndef __SYLAR_MACRO_H__
#define __SYLAR_MACRO_H__

#include <string>
#include <assert.h>
#include "include/log.h"
#include "include/util.h"

#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#   define SYLAR_LIKELY(x)      __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#   define SYLAR_UNLIKELY(x)     __builtin_expect(!!(x), 0)

#else

#   define SYLAR_LIKELY(x)      (x)
#   define SYLAR_UNLIKELY(x)      (x)

#endif

#endif
