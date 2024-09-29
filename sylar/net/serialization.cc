#include <fstream>
#include <sstream>
#include <string.h>
#include <iomanip>
#include <cmath>

#include "../include/log.h"
#include "include/endian.h"
#include "include/serialization.h"

namespace sylar {

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    ByteArray::Node::Node(size_t s)
        :ptr(new char[s])
        ,next(nullptr)
        ,size(s) {
    }

    ByteArray::Node::Node()
        :ptr(nullptr)
        ,next(nullptr)
        ,size(0) {
    }

    ByteArray::Node::~Node() {
        if(ptr) {
            delete[] ptr;
        }
    }

    ByteArray::ByteArray(size_t base_size)
        :m_baseSize(base_size)
        ,m_position(0)
        ,m_capacity(base_size)
        ,m_size(0)
        ,m_endian(SYLAR_BIG_ENDIAN)
        ,m_root(new Node(base_size))
        ,m_cur(m_root) {
    }

    ByteArray::~ByteArray() {
        Node* tmp = m_root;
        while(tmp) {
            m_cur = tmp;
            tmp = tmp->next;
            delete m_cur;
        }
    }

    bool ByteArray::isLittleEndian() const {
        return m_endian == SYLAR_LITTLE_ENDIAN;
    }

    void ByteArray::setIsLittleEndian(bool val) {
        if(val) {
            m_endian = SYLAR_LITTLE_ENDIAN;
        } else {
            m_endian = SYLAR_BIG_ENDIAN;
        }
    }

    void ByteArray::writeFint8(int8_t value) {
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint8(uint8_t value) {
        write(&value, sizeof(value));
    }
    void ByteArray::writeFint16(int16_t value) {
        if(m_endian != SYLAR_BYTE_ORDER) { // 如果当前机器不是大端模式, 则需要进行字节序转换
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint16(uint16_t value) {
        if(m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint32(int32_t value) {
        if(m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint32(uint32_t value) {
        if(m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint64(int64_t value) {
        if(m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint64(uint64_t value) {
        if(m_endian != SYLAR_BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    /// 使用Zigzag编码将int32_t类型的值转换为uint32_t类型的值
    /// 如果v < 0, 则返回((uint32_t)(-v)) * 2 - 1
    /// 否则返回v * 2
    static uint32_t EncodeZigzag32(const int32_t& v) {
        if(v < 0) {
            return ((uint32_t)(-v)) * 2 - 1;
        } else {
            return v * 2;
        }
    }

    /// 使用Zigzag编码将int64_t类型的值转换为uint64_t类型的值
    static uint64_t EncodeZigzag64(const int64_t& v) {
        if(v < 0) {
            return ((uint64_t)(-v)) * 2 - 1;
        } else {
            return v * 2;
        }
    }

    /// 将uint32_t类型的值按照Zigzag编码转换为int32_t类型的值
    /// 如果v & 1 == 0, 则返回(v >> 1)
    /// 否则返回-(v >> 1)
    static int32_t DecodeZigzag32(const uint32_t& v) {
        // 解释：
        // zigzag编码中，对于非负数 n，编码为 2n，解码时右移一位即可得到 n，
        //              对于负数 -n，编码为 2n-1，解码时右移一位得到 n-1，再通过或 -1 操作得到 -(n-1)-1，即 -n
        // v >> 1: 将v右移1位，相当于除以2，对于偶数编码（原始非负数），会得到原始值，对于奇数编码（原始负数），这会得到原始负数的绝对值减 1
        // -(v & 1): 如果v是偶数，则结果为0，如果v是奇数，则结果为-1
        // (v >> 1) ^ -(v & 1): 对于原始非负数，-(v & 1) 为 0，异或操作不改变 v >> 1 的值
        //                      对于原始负数，-(v & 1) 为 -1（所有位为 1），异或操作会将 v >> 1 的所有位取反并减 1
        return (v >> 1) ^ -(v & 1); // 很精妙！！！
    }

    /// 将uint64_t类型的值按照Zigzag编码转换为int64_t类型的值
    static int64_t DecodeZigzag64(const uint64_t& v) {
        return (v >> 1) ^ -(v & 1);
    }

    /// 将int32_t类型的值按照Zigzag编码转换为uint32_t类型的值，然后写入到ByteArray中
    void ByteArray::writeInt32(int32_t value) {
        writeUint32(EncodeZigzag32(value));
    }

    /// 将uint32_t类型的值写入到ByteArray中
    /// 使用Varint编码将uint32_t类型的值转换为uint8_t类型的值，然后写入到ByteArray中
    void ByteArray::writeUint32(uint32_t value) {
        uint8_t tmp[5]; // Varint编码中一个uint32_t类型的值最多需要5个字节来表示
        uint8_t i = 0;
        // 0x80: 1000 0000， 0x7F: 0111 1111，这两个用于提取value的低7位有效数据
        while(value >= 0x80) { // 如果value >= 0x80，则需要将value分成多个字节写入到ByteArray中
            tmp[i++] = (value & 0x7F) | 0x80; // 将value的低7位写入到tmp中，并设置最高位为1
            value >>= 7; // 处理完value的低7位后，将value右移7位，继续处理后续位置
        }
        tmp[i++] = value; // 将value的最低7位写入到tmp中
        write(tmp, i); // 将tmp写入到ByteArray中
    }

    /// 将int64_t类型的值按照Zigzag编码转换为uint64_t类型的值，然后写入到ByteArray中
    void ByteArray::writeInt64(int64_t value) {
        writeUint64(EncodeZigzag64(value));
    }

    /// 将uint64_t类型的值使用Varint编码转换为uint8_t类型的值，然后写入到ByteArray中
    void ByteArray::writeUint64(uint64_t value) {
        uint8_t tmp[10]; // Varint编码中一个uint64_t类型的值最多需要10个字节来表示
        uint8_t i = 0;
        while(value >= 0x80) { 
            tmp[i++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i++] = value;
        write(tmp, i);
    }

    void ByteArray::writeFloat(float value) {
        uint32_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint32(v);
    }

    void ByteArray::writeDouble(double value) {
        uint64_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint64(v);
    }

    void ByteArray::writeStringF16(const std::string& value) {
        writeFuint16(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringF32(const std::string& value) {
        writeFuint32(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringF64(const std::string& value) {
        writeFuint64(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringVint(const std::string& value) {
        writeUint64(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringWithoutLength(const std::string& value) {
        write(value.c_str(), value.size());
    }

    int8_t   ByteArray::readFint8() {
        int8_t v;
        read(&v, sizeof(v));
        return v;
    }

    uint8_t  ByteArray::readFuint8() {
        uint8_t v;
        read(&v, sizeof(v));
        return v;
    }

    /// 使用宏定义，将int16_t、uint16_t、int32_t、uint32_t、int64_t、uint64_t类型的值读取出来
    /// 如果当前机器不是大端模式，则需要进行字节序转换
    #define XX(type) \
        type v; \
        read(&v, sizeof(v)); \
        if(m_endian == SYLAR_BYTE_ORDER) { \
            return v; \
        } else { \
            return byteswap(v); \
        }

    int16_t  ByteArray::readFint16() {
        XX(int16_t);
    }
    uint16_t ByteArray::readFuint16() {
        XX(uint16_t);
    }

    int32_t  ByteArray::readFint32() {
        XX(int32_t);
    }

    uint32_t ByteArray::readFuint32() {
        XX(uint32_t);
    }

    int64_t  ByteArray::readFint64() {
        XX(int64_t);
    }

    uint64_t ByteArray::readFuint64() {
        XX(uint64_t);
    }

    #undef XX

    int32_t  ByteArray::readInt32() {
        return DecodeZigzag32(readUint32());
    }

    
    uint32_t ByteArray::readUint32() {
        uint32_t result = 0;
        for(int i = 0; i < 32; i += 7) {
            uint8_t b = readFuint8();
            if(b < 0x80) {
                result |= ((uint32_t)b) << i;
                break;
            } else {
                result |= (((uint32_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    int64_t  ByteArray::readInt64() {
        return DecodeZigzag64(readUint64());
    }

    uint64_t ByteArray::readUint64() {
        uint64_t result = 0;
        for(int i = 0; i < 64; i += 7) {
            uint8_t b = readFuint8();
            if(b < 0x80) {
                result |= ((uint64_t)b) << i;
                break;
            } else {
                result |= (((uint64_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    float    ByteArray::readFloat() {
        uint32_t v = readFuint32();
        float value;
        memcpy(&value, &v, sizeof(v));
        return value;
    }

    double   ByteArray::readDouble() {
        uint64_t v = readFuint64();
        double value;
        memcpy(&value, &v, sizeof(value));
        return value;
    }

    std::string ByteArray::readStringF16() {
        uint16_t len = readFuint16();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string ByteArray::readStringF32() {
        uint32_t len = readFuint32();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string ByteArray::readStringF64() {
        uint64_t len = readFuint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string ByteArray::readStringVint() {
        uint64_t len = readUint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    void ByteArray::clear() {
        m_position = m_size = 0;
        m_capacity = m_baseSize;
        Node* tmp = m_root->next;
        while(tmp) {
            m_cur = tmp;
            tmp = tmp->next;
            delete m_cur;
        }
        m_cur = m_root;
        m_root->next = NULL;
    }

    /// 将buf中的size个字节写入到ByteArray中
    void ByteArray::write(const void* buf, size_t size) {
        if(size == 0) {
            return;
        }
        addCapacity(size);  // 确保有足够的容量

        size_t npos = m_position % m_baseSize;  // 当前节点内的偏移位置
        size_t ncap = m_cur->size - npos;  // 当前节点剩余可写容量
        size_t bpos = 0;  // 源缓冲区的读取位置
        
        // 如果size大于ncap，则需要将size分成多个字节写入到ByteArray中
        // 如果size小于ncap，则直接将size个字节写入到ByteArray中
        while(size > 0) {
            if(ncap >= size) {  // 当前节点剩余空间足够
                memcpy(m_cur->ptr + npos, (const char*)buf + bpos, size);
                if(m_cur->size == (npos + size)) {
                    m_cur = m_cur->next;  // 如果刚好写满，移动到下一个节点
                }
                m_position += size;
                bpos += size;
                size = 0;
            } else {  // 当前节点剩余空间不足
                memcpy(m_cur->ptr + npos, (const char*)buf + bpos, ncap);
                m_position += ncap;
                bpos += ncap;
                size -= ncap;
                m_cur = m_cur->next;  // 移动到下一个节点
                ncap = m_cur->size;
                npos = 0;  // 新节点从头开始写
            }
        }

        // 如果新位置超过了当前大小，更新总大小
        if(m_position > m_size) {
            m_size = m_position;  
        }
    }

    void ByteArray::read(void* buf, size_t size) {
        if(size > getReadSize()) {
            throw std::out_of_range("not enough len");
        }

        size_t npos = m_position % m_baseSize;  // 当前节点内的偏移位置
        size_t ncap = m_cur->size - npos;  // 当前节点剩余可读容量
        size_t bpos = 0;  // 目标缓冲区的写入位置

        while(size > 0) {
            if(ncap >= size) {  // 当前节点剩余数据足够
                memcpy((char*)buf + bpos, m_cur->ptr + npos, size);
                if(m_cur->size == (npos + size)) {
                    m_cur = m_cur->next;  // 如果刚好读完，移动到下一个节点
                }
                m_position += size;
                bpos += size;
                size = 0;
            } else {  // 当前节点剩余数据不足
                memcpy((char*)buf + bpos, m_cur->ptr + npos, ncap);
                m_position += ncap;
                bpos += ncap;
                size -= ncap;
                m_cur = m_cur->next;  // 移动到下一个节点
                ncap = m_cur->size;
                npos = 0;  // 新节点从头开始读
            }
        }
    }

    void ByteArray::read(void* buf, size_t size, size_t position) const {
        // 检查是否有足够的数据可读
        if(size > (m_size - position)) {
            throw std::out_of_range("not enough len");
        }

        size_t npos = position % m_baseSize;  // 计算在当前节点内的偏移量
        size_t ncap = m_cur->size - npos;     // 当前节点剩余可读容量
        size_t bpos = 0;                      // 目标缓冲区的写入位置
        Node* cur = m_cur;                    // 当前节点

        while(size > 0) {
            if(ncap >= size) {  // 当前节点剩余数据足够
                // 复制数据到目标缓冲区
                memcpy((char*)buf + bpos, cur->ptr + npos, size);
                // 如果刚好读完当前节点，移动到下一个节点
                if(cur->size == (npos + size)) {
                    cur = cur->next;
                }
                position += size;  // 更新位置
                bpos += size;      // 更新目标缓冲区偏移
                size = 0;          // 读取完成
            } else {  // 当前节点剩余数据不足，需要跨节点读取
                // 复制当前节点剩余的数据到目标缓冲区
                memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
                position += ncap;  // 更新位置
                bpos += ncap;      // 更新目标缓冲区偏移
                size -= ncap;      // 更新剩余读取长度
                cur = cur->next;   // 移动到下一个节点
                ncap = cur->size;  // 更新当前节点剩余可读容量
                npos = 0;          // 新节点从头开始读
            }
        }
    }

    void ByteArray::setPosition(size_t v) {
        // 检查设置的位置是否超出总容量
        if(v > m_capacity) {
            throw std::out_of_range("set_position out of range");
        }

        // 更新当前位置
        m_position = v;

        // 如果新位置超过了当前大小，更新总大小
        if(m_position > m_size) {
            m_size = m_position;
        }

        // 重置当前节点指针到根节点
        m_cur = m_root;

        // 遍历链表，找到包含新位置的节点
        while(v > m_cur->size) {
            v -= m_cur->size;  // 减去当前节点的大小
            m_cur = m_cur->next;  // 移动到下一个节点
        }

        // 处理边界情况：如果新位置恰好在节点末尾，移动到下一个节点
        if(v == m_cur->size) {
            m_cur = m_cur->next;
        }

        // 注意：此时 v 表示在当前节点内的偏移量
        // m_cur 指向包含新位置的节点（或下一个节点，如果恰好在节点边界）
    }

    bool ByteArray::writeToFile(const std::string& name) const {
        // 以二进制模式打开文件，如果文件存在则截断
        std::ofstream ofs;
        ofs.open(name, std::ios::trunc | std::ios::binary);
        if(!ofs) {
            SYLAR_LOG_ERROR(g_logger) << "writeToFile name=" << name
                << " error , errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }

        int64_t read_size = getReadSize();  // 获取可读数据大小
        int64_t pos = m_position;  // 当前位置
        Node* cur = m_cur;  // 当前节点

        while(read_size > 0) {
            int diff = pos % m_baseSize;  // 计算在当前节点内的偏移量
            // 计算本次写入长度，不超过节点大小和剩余数据量
            int64_t len = (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;
            ofs.write(cur->ptr + diff, len);  // 写入数据
            cur = cur->next;  // 移动到下一个节点
            pos += len;  // 更新位置
            read_size -= len;  // 更新剩余数据量
        }

        return true;
    }

    bool ByteArray::readFromFile(const std::string& name) {
        // 以二进制模式打开文件
        std::ifstream ifs;
        ifs.open(name, std::ios::binary);
        if(!ifs) {
            SYLAR_LOG_ERROR(g_logger) << "readFromFile name=" << name
                << " error, errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }

        // 创建一个智能指针管理的缓冲区，大小为m_baseSize
        // 使用lambda表达式在智能指针销毁时释放缓冲区
        std::shared_ptr<char> buff(new char[m_baseSize], [](char* ptr) { delete[] ptr;});
        while(!ifs.eof()) {
            ifs.read(buff.get(), m_baseSize);  // 从文件读取数据到缓冲区
            write(buff.get(), ifs.gcount());  // 将读取的数据写入ByteArray
        }
        return true;
    }

    void ByteArray::addCapacity(size_t size) {
        if(size == 0) {
            return;
        }
        size_t old_cap = getCapacity();
        if(old_cap >= size) {
            return;  // 如果现有容量足够，无需增加
        }

        size = size - old_cap;  // 计算需要增加的容量
        size_t count = ceil(1.0 * size / m_baseSize);  // 计算需要增加的节点数

        // 找到最后一个节点
        Node* tmp = m_root;
        while(tmp->next) {
            tmp = tmp->next;
        }

        Node* first = NULL;
        // 添加新节点
        for(size_t i = 0; i < count; ++i) {
            tmp->next = new Node(m_baseSize);
            if(first == NULL) {
                first = tmp->next;  // 记录第一个新节点
            }
            tmp = tmp->next;
            m_capacity += m_baseSize;
        }

        if(old_cap == 0) {
            m_cur = first;  // 如果之前容量为0，设置当前节点为新添加的第一个节点
        }
    }

    std::string ByteArray::toString() const {
        std::string str;
        str.resize(getReadSize());  // 预分配足够的空间
        if(str.empty()) {
            return str;
        }
        // 从当前位置读取数据到字符串
        read(&str[0], str.size(), m_position);
        return str;
    }

    std::string ByteArray::toHexString() const {
        std::string str = toString();
        std::stringstream ss;

        // 将每个字节转换为两位十六进制表示
        for(size_t i = 0; i < str.size(); ++i) {
            if(i > 0 && i % 32 == 0) {
                ss << std::endl;  // 每32个字节换行
            }
            // 设置宽度为2，填充字符为'0'，使用十六进制表示
            ss << std::setw(2) << std::setfill('0') << std::hex
               << (int)(uint8_t)str[i] << " ";
        }

        return ss.str();
    }


    uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
        // 确保不会读取超过可读数据的长度
        len = len > getReadSize() ? getReadSize() : len;
        if(len == 0) {
            return 0;
        }

        uint64_t size = len;  // 保存原始请求长度

        size_t npos = m_position % m_baseSize;  // 当前节点内的偏移量
        size_t ncap = m_cur->size - npos;  // 当前节点剩余可读容量
        struct iovec iov;
        Node* cur = m_cur;

        while(len > 0) {
            if(ncap >= len) {  // 当前节点剩余数据足够
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {  // 需要跨节点读取
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.emplace_back(iov);
        }
        return size;  // 返回实际读取的长度
    }

    uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers
                                    ,uint64_t len, uint64_t position) const {
        // 类似于上面的函数，但从指定位置开始读取
        len = len > getReadSize() ? getReadSize() : len;
        if(len == 0) {
            return 0;
        }

        uint64_t size = len;

        // 计算起始节点和节点内偏移量
        size_t npos = position % m_baseSize;
        size_t count = position / m_baseSize;
        Node* cur = m_root;
        while(count > 0) {
            cur = cur->next;
            --count;
        }

        size_t ncap = cur->size - npos;
        struct iovec iov;
        while(len > 0) {
            if(ncap >= len) {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.emplace_back(iov);
        }
        return size;
    }

    uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
        if(len == 0) {
            return 0;
        }
        addCapacity(len);  // 确保有足够的写入空间
        uint64_t size = len;

        size_t npos = m_position % m_baseSize;  // 当前节点内的偏移量
        size_t ncap = m_cur->size - npos;  // 当前节点剩余可写容量
        struct iovec iov;
        Node* cur = m_cur;
        while(len > 0) {
            if(ncap >= len) {  // 当前节点剩余空间足够
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {  // 需要跨节点写入
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;

                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.emplace_back(iov);
        }
        return size;  // 返回实际可写入的长度
    }

}