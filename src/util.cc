/**
 * @file util.cc
 * @author beanljun
 * @brief 
 * @date 2024-04-25
 */

#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <signal.h> //kill()
#include <execinfo.h> //backtrace()
#include <cxxabi.h> //abi::__cxa_demangle()
#include <algorithm> //std::transform

#include "include/util.h"
#include "include/log.h"
#include "include/fiber.h"


namespace sylar {

pid_t GetThreadId() { return syscall(SYS_gettid); }

uint64_t GetFiberId() { return Fiber::GetFiberId(); }

uint64_t GetElapsedMS() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec*1000 + ts.tv_nsec/1000000;
}

std::string GetThreadName() { 
    char thread_name[16] = {0};
    pthread_getname_np(pthread_self(), thread_name, 16);
    return std::string(thread_name);
}

void SetThreadName(const std::string& name) {
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
}

static std::string demangle(const char* str) {
    size_t size = 0;
    int status  = 0;
    std::string rt;
    rt.resize(256);
    if (1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0])) {
        char *v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
        if (v) {
            std::string result(v);
            free(v);
            return result;
        }
    }
    if (1 == sscanf(str, "%255s", &rt[0])) return rt;
    return str;
}

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    void** array = (void **)malloc((sizeof(void*)) * size);
    size_t s = ::backtrace(array, size); //获取当前线程的调用栈

    char** strings = backtrace_symbols(array, s);
    if(strings == NULL) {
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "backtrace_symbols error";
        return;
    }

    for(size_t i = skip; i < s; ++i) bt.push_back(demangle(strings[i]));

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for(auto& i : bt) ss << prefix << i << std::endl;
    return ss.str();
}

uint64_t GetCurrentMS() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

uint64_t GetCurrentUS() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000ul + tv.tv_usec;
}

std::string ToUpper(const std::string& name) {
    std::string rt = name;
    std::transform(rt.begin(), rt.end(), rt.begin(), ::toupper); //容器开始，容器结束，目标容器开始，转换函数
    return rt;
}

std::string ToLower(const std::string& name) {
    std::string rt = name;
    std::transform(rt.begin(), rt.end(), rt.begin(), ::tolower);
    return rt;
}

std::string Time2Str(time_t ts, const std::string& format) {
    struct tm tm;
    localtime_r(&ts, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), format.c_str(), &tm);
    return buf;
}

time_t Str2Time(const char* str, const char* format) {
    struct tm t;
    memset(&t, 0, sizeof(t));   //将t的前sizeof(t)个字节用0替换
    if(!strptime(str, format, &t)) return 0;// strptime(str, format, &t), 将str按照format格式解析到t中
    return mktime(&t);
}

void FSUtil::ListAllFile(std::vector<std::string> &files, const std::string& path, const std::string& subfix) {
    if(access(path.c_str(), 0) != 0) return; //检查文件是否存在,或者是否有权限访问
    
    DIR *dir = opendir(path.c_str());   //打开目录，返回目录流指针，失败返回NULL
    if(dir == nullptr) return;

    struct dirent *dp = nullptr;
    while((dp = readdir(dir)) != nullptr) {
        if(dp -> d_type == DT_DIR) { //如果是目录
            if(!strcmp(dp -> d_name, ".") || !strcmp(dp -> d_name, "..")) continue; //如果是当前目录或者上级目录，则跳过
            ListAllFile(files, path + "/" + dp -> d_name, subfix);//递归遍历
        } else if(dp -> d_type == DT_REG) {//如果是常规文件
            std::string filename(dp -> d_name);
            if(subfix.empty()) files.push_back(path + "/" + filename);//如果没有指定后缀，则将文件路径加入文件列表
            else if(filename.size() < subfix.size()) continue;//如果文件名长度小于后缀长度，则跳过
            else if(filename.substr(filename.size() - subfix.size()) == subfix) files.push_back(path + "/" + filename);//如果文件名后缀与指定后缀相同，则将文件路径加入文件列表
        }
    }
    closedir(dir);//关闭目录流
}

static int __lstat(const char* file, struct stat* st) {
    struct stat lst;
    int rt = lstat(file, &lst); 
    if(st) *st = lst;  //将lst的内容拷贝到st中
    return rt;
}

static int __mkdir(const char* dir) {
    if(access(dir, F_OK) == 0) return 0; //检查目录是否存在, 如果存在返回0
    return mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); //创建目录
}

bool FSUtil::Mkdir(const std::string& dirname) {
    if(__lstat(dirname.c_str(), nullptr) == 0) return true; //如果目录已经存在，则返回true

    char* path = strdup(dirname.c_str());   //将dirname拷贝到path中
    char* ptr = strchr(path + 1, '/');      //查找path中第一个/的位置

    do {   //创建目录，如果子目录不存在则创建子目录，创建失败时跳出循环
        for(; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {    
            *ptr = '\0';                                        //将ptr指向的位置设置为\0
            if(__mkdir(path) != 0) break;                       //创建目录, 如果失败则跳出循环
        }
        if(ptr != nullptr) break;                               //如果ptr不为空，则跳出循环
        else if(__mkdir(path) != 0) break;                      //如果创建目录失败，则跳出循环
        free(path);
        return true;
    } while (0);

    free(path);
    return false;
}

bool FSUtil::IsRunningPidfile(const std::string& pidfile) {
    if(__lstat(pidfile.c_str(), nullptr) != 0) return false; //如果pid文件不存在，则返回false

    std::ifstream ifs(pidfile); //以只读方式打开文件
    std::string line;
    if(!ifs || !std::getline(ifs, line)) return false;  //如果文件打开失败或者读取失败，则返回false
    if(line.empty()) return false;                      //如果文件内容为空，则返回false

    pid_t pid = atoi(line.c_str());             //将字符串转换为整数
    if(pid <= 1) return false;                  //0号进程是内核进程，1号进程是init进程，所以pid小于等于直接返回false
    if(kill(pid, 0) != 0) return false;         //如果进程不存在，则返回false

    return true;
}

bool FSUtil::Unlink(const std::string& filename, bool exist) {
    if(!exist && __lstat(filename.c_str(), nullptr) != 0) return true; //如果文件不存在，无需删除，返回true
    return unlink(filename.c_str()) == 0; //删除文件，成功0 == 0，返回true
}

bool FSUtil::Rm(const std::string& path) {
    struct stat st;
    if(lstat(path.c_str(), &st) != 0) return true; //如果文件不存在，无需删除，返回true

    DIR* dir = opendir(path.c_str());
    if(dir == nullptr) return false; //打开目录失败，返回false

    bool ret = true;
    struct dirent* dp = nullptr;    //目录项

    while((dp = readdir(dir)) != nullptr) {
        if(!strcmp(dp -> d_name, ".") || !strcmp(dp -> d_name, "..")) continue; //如果是当前目录或者上级目录，则跳过
        std::string filepath = path + "/" + dp -> d_name; //文件路径
        if(dp -> d_type == DT_DIR) ret = Rm(filepath); //如果是目录，则递归删除
    }

    closedir(dir); //关闭目录流
    if(::rmdir(path.c_str())) ret = false;

    return ret;
}

bool FSUtil::Mv(const std::string& from, const std::string& to) {
    if(!Rm(to)) return false; //如果目标文件存在，返回false
    return rename(from.c_str(), to.c_str()) == 0; 
}

bool FSUtil::Realpath(const std::string& path, std::string& rpath) {
    if(__lstat(path.c_str(), nullptr)) return false; //如果文件不存在，返回false

    char* ptr = ::realpath(path.c_str(), nullptr); //获取绝对路径
    if(ptr == nullptr) return false; //获取失败，返回false

    std::string(ptr).swap(rpath); //交换ptr和rpath的内容
    free(ptr);
    return true;
}

bool FSUtil::Symlink(const std::string& from, const std::string& to) {
    if(!Rm(to)) return false; //如果目标文件存在，返回false
    return ::symlink(from.c_str(), to.c_str()) == 0; //创建符号链接
}

std::string FSUtil::Dirname(const std::string& filename) {
    if(filename.empty()) return ".";
    auto pos = filename.rfind('/'); //查找最后一个/的位置
    if(pos == 0) return "/";
    else if(pos == std::string::npos) return ".";
    return filename.substr(0, pos); //返回文件的目录名
}

std::string FSUtil::Basename(const std::string& filename) {
    if(filename.empty()) return ".";
    auto pos = filename.rfind('/'); 
    if(pos == std::string::npos) return filename;
    return filename.substr(pos + 1); 
}

bool FSUtil::OpenForRead(std::ifstream& ifs, const std::string& filename, std::ios_base::openmode mode) {
    ifs.open(filename.c_str(), mode);
    return ifs.is_open();
}

bool FSUtil::OpenForWrite(std::ofstream& ofs, const std::string& filename, std::ios_base::openmode mode) {
    ofs.open(filename.c_str(), mode);
    if(!ofs.is_open()) {
        FSUtil::Mkdir(FSUtil::Dirname(filename));
        ofs.open(filename.c_str(), mode);
    }
    return ofs.is_open();
}

int8_t TypeUtil::ToChar(const char* str) {
    if(str == nullptr) return 0;
    return str[0];
}

int8_t TypeUtil::ToChar(const std::string& str) {
    if(str.empty()) return 0;
    return *str.begin();
}

int64_t TypeUtil::Atoi(const char* str) {
    if(str == nullptr) return 0;
    return strtoull(str, nullptr, 10); // 字符串， 字符串结束位置， 进制， 返回字符串转换后的整数
}

int64_t TypeUtil::Atoi(const std::string& str) {
    if(str.empty()) return 0;
    return strtoull(str.c_str(), nullptr, 10);
}

double TypeUtil::Atof(const char* str) {
    if(str == nullptr) return 0;
    return atof(str);
}

double TypeUtil::Atof(const std::string& str) {
    if(str.empty()) return 0;
    return atof(str.c_str());
}

}  // namespace sylar
