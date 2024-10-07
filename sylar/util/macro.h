/**
 * @file macro.h
 * @author beanljun
 * @brief 常用宏定义
 * @date 2024-04-25
 */

#ifndef __MACRO_H__
#define __MACRO_H__

#include <assert.h>

#include "../include/log.h"

#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#define SYLAR_LIKELY(x) __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#define SYLAR_UNLIKELY(x) __builtin_expect(!!(x), 0)

#else

#define SYLAR_LIKELY(x) (x)
#define SYLAR_UNLIKELY(x) (x)

#endif

/// 断言宏开关，默认关闭
#define SYLAR_ASSERT_ON false
/// 断言宏封装，如果条件不成立，打印日志并终止程序
#define SYLAR_ASSERT(x)                                                                    \
    if (SYLAR_ASSERT_ON) {                                                                 \
        if (SYLAR_UNLIKELY(!(x))) {                                                        \
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x << "\nbacktrace:\n"      \
                                              << sylar::BacktraceToString(100, 2, "    "); \
            assert(x);                                                                     \
        }                                                                                  \
    }

/// 断言宏封装，如果条件不成立，打印日志并终止程序，并且附加断言信息
#define SYLAR_ASSERT2(x, w)                                                                \
    if (SYLAR_ASSERT_ON) {                                                                 \
        if (SYLAR_UNLIKELY(!(x))) {                                                        \
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x << "\n"                  \
                                              << w << "\nbacktrace:\n"                     \
                                              << sylar::BacktraceToString(100, 2, "    "); \
            assert(x);                                                                     \
        }                                                                                  \
    }

#endif
