/**
 * @file daemon.h
 * @brief 守护进程
 * @date 2024-10-06
 */

#ifndef __DAEMON_H__
#define __DAEMON_H__

#include <unistd.h>

#include <functional>

#include "../util/singleton.h"

namespace sylar {

struct ProcessInfo {
    pid_t    parent_id = 0;          // 父进程id
    pid_t    main_id = 0;            // 主进程id
    uint64_t parent_start_time = 0;  // 父进程启动时间
    uint64_t main_start_time = 0;    // 主进程启动时间
    uint32_t restart_count = 0;      // 主进程重启的次数

    std::string toString() const;
};

typedef sylar::Singleton<ProcessInfo> ProcessInfoMgr;

/**
 * @brief 启动守护进程, 父进程退出后, 子进程会自动转成守护进程
 * @param[in] argc 参数个数
 * @param[in] argv 参数值数组
 * @param[in] main_cb 启动函数
 * @param[in] is_daemon 是否守护进程的方式
 * @return 返回程序的执行结果
 */
int start_daemon(int argc, char** argv, std::function<int(int argc, char** argv)> main_cb, bool is_daemon);

}  // namespace sylar

#endif
