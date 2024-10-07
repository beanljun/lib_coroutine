/**
 * @file util.cc
 * @author beanljun
 * @brief
 * @date 2024-04-25
 */

#include "../util/util.h"

#include <cxxabi.h>  //abi::__cxa_demangle()
#include <dirent.h>
#include <execinfo.h>  //backtrace()
#include <signal.h>    //kill()
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>  //std::transform

#include "../include/fiber.h"
#include "../include/log.h"


namespace sylar {

pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

uint64_t GetFiberId() {
    return Fiber::GetFiberId();
}

uint64_t GetElapsedMS() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
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
    size_t      size = 0;
    int         status = 0;
    std::string rt;
    rt.resize(256);
    if (1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0])) {
        char* v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
        if (v) {
            std::string result(v);
            free(v);
            return result;
        }
    }
    if (1 == sscanf(str, "%255s", &rt[0]))
        return rt;
    return str;
}

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    void** array = (void**)malloc((sizeof(void*)) * size);
    size_t s = ::backtrace(array, size);  //获取当前线程的调用栈

    char** strings = backtrace_symbols(array, s);
    if (strings == NULL) {
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "backtrace_symbols error";
        return;
    }

    for (size_t i = skip; i < s; ++i)
        bt.push_back(demangle(strings[i]));

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for (auto& i : bt)
        ss << prefix << i << std::endl;
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
    std::transform(rt.begin(), rt.end(), rt.begin(),
                   ::toupper);  //容器开始，容器结束，目标容器开始，转换函数
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
    memset(&t, 0, sizeof(t));  //将t的前sizeof(t)个字节用0替换
    if (!strptime(str, format, &t))
        return 0;  // strptime(str, format, &t), 将str按照format格式解析到t中
    return mktime(&t);
}

void FSUtil::ListAllFile(std::vector<std::string>& files, const std::string& path, const std::string& subfix) {
    if (access(path.c_str(), 0) != 0)
        return;  //检查文件是否存在,或者是否有权限访问

    DIR* dir = opendir(path.c_str());  //打开目录，返回目录流指针，失败返回NULL
    if (dir == nullptr)
        return;

    struct dirent* dp = nullptr;
    while ((dp = readdir(dir)) != nullptr) {
        if (dp->d_type == DT_DIR) {  //如果是目录
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
                continue;                                         //如果是当前目录或者上级目录，则跳过
            ListAllFile(files, path + "/" + dp->d_name, subfix);  //递归遍历
        } else if (dp->d_type == DT_REG) {                        //如果是常规文件
            std::string filename(dp->d_name);
            if (subfix.empty())
                files.push_back(path + "/" + filename);  //如果没有指定后缀，则将文件路径加入文件列表
            else if (filename.size() < subfix.size())
                continue;  //如果文件名长度小于后缀长度，则跳过
            else if (filename.substr(filename.size() - subfix.size()) == subfix)
                files.push_back(path + "/" + filename);  //如果文件名后缀与指定后缀相同，则将文件路径加入文件列表
        }
    }
    closedir(dir);  //关闭目录流
}

static int __lstat(const char* file, struct stat* st) {
    struct stat lst;
    int         rt = lstat(file, &lst);
    if (st)
        *st = lst;  //将lst的内容拷贝到st中
    return rt;
}

static int __mkdir(const char* dir) {
    if (access(dir, F_OK) == 0)
        return 0;                                              //检查目录是否存在, 如果存在返回0
    return mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);  //创建目录
}

bool FSUtil::Mkdir(const std::string& dirname) {
    if (__lstat(dirname.c_str(), nullptr) == 0)
        return true;  //如果目录已经存在，则返回true

    char* path = strdup(dirname.c_str());  //将dirname拷贝到path中
    char* ptr = strchr(path + 1, '/');     //查找path中第一个/的位置

    do {  //创建目录，如果子目录不存在则创建子目录，创建失败时跳出循环
        for (; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {
            *ptr = '\0';  //将ptr指向的位置设置为\0
            if (__mkdir(path) != 0)
                break;  //创建目录, 如果失败则跳出循环
        }
        if (ptr != nullptr)
            break;  //如果ptr不为空，则跳出循环
        else if (__mkdir(path) != 0)
            break;  //如果创建目录失败，则跳出循环
        free(path);
        return true;
    } while (0);

    free(path);
    return false;
}

bool FSUtil::IsRunningPidfile(const std::string& pidfile) {
    if (__lstat(pidfile.c_str(), nullptr) != 0)
        return false;  //如果pid文件不存在，则返回false

    std::ifstream ifs(pidfile);  //以只读方式打开文件
    std::string   line;
    if (!ifs || !std::getline(ifs, line))
        return false;  //如果文件打开失败或者读取失败，则返回false
    if (line.empty())
        return false;  //如果文件内容为空，则返回false

    pid_t pid = atoi(line.c_str());  //将字符串转换为整数
    if (pid <= 1)
        return false;  // 0号进程是内核进程，1号进程是init进程，所以pid小于等于直接返回false
    if (kill(pid, 0) != 0)
        return false;  //如果进程不存在，则返回false

    return true;
}

bool FSUtil::Unlink(const std::string& filename, bool exist) {
    if (!exist && __lstat(filename.c_str(), nullptr) != 0)
        return true;                       //如果文件不存在，无需删除，返回true
    return unlink(filename.c_str()) == 0;  //删除文件，成功0 == 0，返回true
}

bool FSUtil::Rm(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0)
        return true;  //如果文件不存在，无需删除，返回true

    DIR* dir = opendir(path.c_str());
    if (dir == nullptr)
        return false;  //打开目录失败，返回false

    bool           ret = true;
    struct dirent* dp = nullptr;  //目录项

    while ((dp = readdir(dir)) != nullptr) {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            continue;                                    //如果是当前目录或者上级目录，则跳过
        std::string filepath = path + "/" + dp->d_name;  //文件路径
        if (dp->d_type == DT_DIR)
            ret = Rm(filepath);  //如果是目录，则递归删除
    }

    closedir(dir);  //关闭目录流
    if (::rmdir(path.c_str()))
        ret = false;

    return ret;
}

bool FSUtil::Mv(const std::string& from, const std::string& to) {
    if (!Rm(to))
        return false;  //如果目标文件存在，返回false
    return rename(from.c_str(), to.c_str()) == 0;
}

bool FSUtil::Realpath(const std::string& path, std::string& rpath) {
    if (__lstat(path.c_str(), nullptr))
        return false;  //如果文件不存在，返回false

    char* ptr = ::realpath(path.c_str(), nullptr);  //获取绝对路径
    if (ptr == nullptr)
        return false;  //获取失败，返回false

    std::string(ptr).swap(rpath);  //交换ptr和rpath的内容
    free(ptr);
    return true;
}

bool FSUtil::Symlink(const std::string& from, const std::string& to) {
    if (!Rm(to))
        return false;                                 //如果目标文件存在，返回false
    return ::symlink(from.c_str(), to.c_str()) == 0;  //创建符号链接
}

std::string FSUtil::Dirname(const std::string& filename) {
    if (filename.empty())
        return ".";
    auto pos = filename.rfind('/');  //查找最后一个/的位置
    if (pos == 0)
        return "/";
    else if (pos == std::string::npos)
        return ".";
    return filename.substr(0, pos);  //返回文件的目录名
}

std::string FSUtil::Basename(const std::string& filename) {
    if (filename.empty())
        return ".";
    auto pos = filename.rfind('/');
    if (pos == std::string::npos)
        return filename;
    return filename.substr(pos + 1);
}

bool FSUtil::OpenForRead(std::ifstream& ifs, const std::string& filename, std::ios_base::openmode mode) {
    ifs.open(filename.c_str(), mode);
    return ifs.is_open();
}

bool FSUtil::OpenForWrite(std::ofstream& ofs, const std::string& filename, std::ios_base::openmode mode) {
    ofs.open(filename.c_str(), mode);
    if (!ofs.is_open()) {
        FSUtil::Mkdir(FSUtil::Dirname(filename));
        ofs.open(filename.c_str(), mode);
    }
    return ofs.is_open();
}

int8_t TypeUtil::ToChar(const char* str) {
    if (str == nullptr)
        return 0;
    return str[0];
}

int8_t TypeUtil::ToChar(const std::string& str) {
    if (str.empty())
        return 0;
    return *str.begin();
}

int64_t TypeUtil::Atoi(const char* str) {
    if (str == nullptr)
        return 0;
    return strtoull(str, nullptr,
                    10);  // 字符串， 字符串结束位置， 进制， 返回字符串转换后的整数
}

int64_t TypeUtil::Atoi(const std::string& str) {
    if (str.empty())
        return 0;
    return strtoull(str.c_str(), nullptr, 10);
}

double TypeUtil::Atof(const char* str) {
    if (str == nullptr)
        return 0;
    return atof(str);
}

double TypeUtil::Atof(const std::string& str) {
    if (str.empty())
        return 0;
    return atof(str.c_str());
}

std::string StringUtil::Format(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto v = Formatv(fmt, ap);
    va_end(ap);
    return v;
}

std::string StringUtil::Formatv(const char* fmt, va_list ap) {
    char* buf = nullptr;
    auto  len = vasprintf(&buf, fmt, ap);
    if (len == -1) {
        return "";
    }
    std::string ret(buf, len);
    free(buf);
    return ret;
}

static const char uri_chars[256] = {
    /* 0 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    /* 64 */
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    /* 128 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 192 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const char xdigit_chars[256] = {
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define CHAR_IS_UNRESERVED(c) (uri_chars[(unsigned char)(c)])

//-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~
std::string StringUtil::UrlEncode(const std::string& str, bool space_as_plus) {
    static const char* hexdigits = "0123456789ABCDEF";
    std::string*       ss = nullptr;
    const char*        end = str.c_str() + str.length();
    for (const char* c = str.c_str(); c < end; ++c) {
        if (!CHAR_IS_UNRESERVED(*c)) {
            if (!ss) {
                ss = new std::string;
                ss->reserve(str.size() * 1.2);
                ss->append(str.c_str(), c - str.c_str());
            }
            if (*c == ' ' && space_as_plus) {
                ss->append(1, '+');
            } else {
                ss->append(1, '%');
                ss->append(1, hexdigits[(uint8_t)*c >> 4]);
                ss->append(1, hexdigits[*c & 0xf]);
            }
        } else if (ss) {
            ss->append(1, *c);
        }
    }
    if (!ss) {
        return str;
    } else {
        std::string rt = *ss;
        delete ss;
        return rt;
    }
}

std::string StringUtil::UrlDecode(const std::string& str, bool space_as_plus) {
    std::string* ss = nullptr;
    const char*  end = str.c_str() + str.length();
    for (const char* c = str.c_str(); c < end; ++c) {
        if (*c == '+' && space_as_plus) {
            if (!ss) {
                ss = new std::string;
                ss->append(str.c_str(), c - str.c_str());
            }
            ss->append(1, ' ');
        } else if (*c == '%' && (c + 2) < end && isxdigit(*(c + 1)) && isxdigit(*(c + 2))) {
            if (!ss) {
                ss = new std::string;
                ss->append(str.c_str(), c - str.c_str());
            }
            ss->append(1, (char)(xdigit_chars[(int)*(c + 1)] << 4 | xdigit_chars[(int)*(c + 2)]));
            c += 2;
        } else if (ss) {
            ss->append(1, *c);
        }
    }
    if (!ss) {
        return str;
    } else {
        std::string rt = *ss;
        delete ss;
        return rt;
    }
}

std::string StringUtil::Trim(const std::string& str, const std::string& delimit) {
    auto begin = str.find_first_not_of(delimit);
    if (begin == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(delimit);
    return str.substr(begin, end - begin + 1);
}

std::string StringUtil::TrimLeft(const std::string& str, const std::string& delimit) {
    auto begin = str.find_first_not_of(delimit);
    if (begin == std::string::npos) {
        return "";
    }
    return str.substr(begin);
}

std::string StringUtil::TrimRight(const std::string& str, const std::string& delimit) {
    auto end = str.find_last_not_of(delimit);
    if (end == std::string::npos) {
        return "";
    }
    return str.substr(0, end);
}

std::string StringUtil::WStringToString(const std::wstring& ws) {
    std::string    str_locale = setlocale(LC_ALL, "");
    const wchar_t* wch_src = ws.c_str();
    size_t         n_dest_size = wcstombs(NULL, wch_src, 0) + 1;
    char*          ch_dest = new char[n_dest_size];
    memset(ch_dest, 0, n_dest_size);
    wcstombs(ch_dest, wch_src, n_dest_size);
    std::string str_result = ch_dest;
    delete[] ch_dest;
    setlocale(LC_ALL, str_locale.c_str());
    return str_result;
}

std::wstring StringUtil::StringToWString(const std::string& s) {
    std::string str_locale = setlocale(LC_ALL, "");
    const char* chSrc = s.c_str();
    size_t      n_dest_size = mbstowcs(NULL, chSrc, 0) + 1;
    wchar_t*    wch_dest = new wchar_t[n_dest_size];
    wmemset(wch_dest, 0, n_dest_size);
    mbstowcs(wch_dest, chSrc, n_dest_size);
    std::wstring wstr_result = wch_dest;
    delete[] wch_dest;
    setlocale(LC_ALL, str_locale.c_str());
    return wstr_result;
}


}  // namespace sylar
