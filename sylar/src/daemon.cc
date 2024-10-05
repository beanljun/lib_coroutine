/**
 * @file daemon.cc
 * @brief 守护进程启动实现
 * @date 2024-10-06
 */
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/daemon.h"
#include "../include/log.h"
#include "../include/config.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
static sylar::ConfigVar<uint32_t>::ptr g_daemon_restart_interval
    = sylar::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");

std::string ProcessInfo::toString() const {
    std::stringstream ss;
    ss << "[ProcessInfo parent_id=" << parent_id
       << " main_id=" << main_id
       << " parent_start_time=" << sylar::Time2Str(parent_start_time)
       << " main_start_time=" << sylar::Time2Str(main_start_time)
       << " restart_count=" << restart_count << "]";
    return ss.str();
}

// 真正的启动函数
static int real_start(int argc, char** argv,
                     std::function<int(int argc, char** argv)> main_cb) {
    return main_cb(argc, argv);
}

// 真正的守护进程函数
static int real_daemon(int argc, char** argv,
                     std::function<int(int argc, char** argv)> main_cb) {
    /*
    daemon(int nochdir, int noclose)
        nochdir：
            如果为 0，将当前工作目录更改为根目录 "/"。
            如果非 0（在这里是 1），保持当前工作目录不变。
        noclose：
            如果为 0（在这里是 0），将标准输入、标准输出和标准错误重定向到 /dev/null。
            如果非 0，保持这些文件描述符不变。
    daemon(1, 0) 的效果是：
        保持当前工作目录不变。将标准输入、标准输出和标准错误重定向到 /dev/null。

    调用 daemon(1, 0) 后，进程会经历以下变化：
        1、创建一个新的会话（session），使进程脱离控制终端。
        2、将进程的工作目录设置为根目录（但在这里因为第一个参数是 1，所以不改变）。
        3、重设文件创建掩码（umask）为 0。
        4、关闭所有打开的文件描述符（但在这里标准输入、输出和错误会重定向到 /dev/null）。
    目的：
        1、使进程在后台运行，不受终端的影响。
        2、确保进程不会因为终端关闭而被终止。
        3、防止进程输出信息到终端，干扰用户的其他操作。
    */
    daemon(1, 0);
    ProcessInfoMgr::GetInstance()->parent_id = getpid();
    ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
    while(true) {
        pid_t pid = fork(); // 创建子进程
        if(pid == 0) {      // 子进程返回
            ProcessInfoMgr::GetInstance()->main_id = getpid();
            ProcessInfoMgr::GetInstance()->main_start_time  = time(0);
            SYLAR_LOG_INFO(g_logger) << "process start pid=" << getpid();
            return real_start(argc, argv, main_cb);
        } else if(pid < 0) { // 创建失败
            SYLAR_LOG_ERROR(g_logger) << "fork fail return=" << pid
                << " errno=" << errno << " errstr=" << strerror(errno);
            return -1;
        } else {            // 父进程返回
            int status = 0;
            waitpid(pid, &status, 0); // 等待子进程结束
            if(status) { // 如果非0, 表示子进程异常退出
                SYLAR_LOG_ERROR(g_logger) << "child crash pid=" << pid
                    << " status=" << status;
            } else { // 如果为0, 表示子进程正常退出
                SYLAR_LOG_INFO(g_logger) << "child finished pid=" << pid;
                break; // 退出循环
            }
            ProcessInfoMgr::GetInstance()->restart_count += 1;  // 重启次数加1
            sleep(g_daemon_restart_interval->getValue());       // 休眠一段时间后重启
        }
    }
    return 0;
}

int start_daemon(int argc, char** argv
                 , std::function<int(int argc, char** argv)> main_cb
                 , bool is_daemon) {
    if(!is_daemon) {
        // 不是守护进程, 直接调用真正的启动函数
        return real_start(argc, argv, main_cb);
    }
    // 是守护进程, 调用真正的守护进程函数
    return real_daemon(argc, argv, main_cb);
}

}
