/**
 * @file serialization.h
 * @brief 序列化/反序列化(二进制数组)
 * @author beanljun
 * @date 2024-07-20
 */
#ifndef __SERIALIZATION_H__
#define __SERIALIZATION_H__

#include <vector>
#include <string>
#include <memory>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace sylar {

/// 二进制数组,提供基础类型的序列化,反序列化功能
class ByteArray {
public:
    typedef std::shared_ptr<ByteArray> ptr;

    // 二进制数组的存储节点
    struct Node {
        /// 构造指定大小的内存块, s 为内存块大小
        Node(size_t s);

        /// 无参构造函数
        Node();

        /// 析构函数,释放内存
        ~Node();

        /// 内存块地址指针
        char* ptr;
        /// 下一个内存块地址
        Node* next;
        /// 内存块大小
        size_t size;
    };

    // 使用指定长度的内存块构造ByteArray，默认大小为4096
    ByteArray(size_t base_size = 4096);

    ~ByteArray();

    /// 写入固定长度int8_t类型的数据，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFint8(int8_t value);

    /// 写入固定长度uint8_t类型的数据，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFuint8(uint8_t value);

    /// 写入固定长度int16_t类型的数据(大端/小端)，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFint16(int16_t value);

    /// 写入固定长度uint16_t类型的数据(大端/小端)，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFuint16(uint16_t value);

    /// 写入固定长度int32_t类型的数据(大端/小端)，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFint32(int32_t value);

    /// 写入固定长度uint32_t类型的数据(大端/小端)，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFuint32(uint32_t value);

    /// 写入固定长度int64_t类型的数据(大端/小端)，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFint64(int64_t value);

    /// 写入固定长度uint64_t类型的数据(大端/小端)，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFuint64(uint64_t value);

    /// 写入有符号Varint32类型的数据，m_position += 实际占用内存(1 ~ 5)，如果m_position > m_size 则 m_size = m_position
    void writeInt32(int32_t value);

    /// 写入无符号Varint32类型的数据，m_position += 实际占用内存(1 ~ 5)，如果m_position > m_size 则 m_size = m_position
    void writeUint32(uint32_t value);

    /// 写入有符号Varint64类型的数据，m_position += 实际占用内存(1 ~ 10)，如果m_position > m_size 则 m_size = m_position
    void writeInt64(int64_t value);

    /// 写入无符号Varint64类型的数据，m_position += 实际占用内存(1 ~ 10)，如果m_position > m_size 则 m_size = m_position
    void writeUint64(uint64_t value);

    /// 写入float类型的数据，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeFloat(float value);

    /// 写入double类型的数据，m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
    void writeDouble(double value);

    /// 写入std::string类型的数据,用uint16_t作为长度类型，m_position += 2 + value.size()，如果m_position > m_size 则 m_size = m_position
    void writeStringF16(const std::string& value);

    /// 写入std::string类型的数据,用uint32_t作为长度类型，m_position += 4 + value.size()，如果m_position > m_size 则 m_size = m_position
    void writeStringF32(const std::string& value);

    /// 写入std::string类型的数据,用uint64_t作为长度类型，m_position += 8 + value.size()，如果m_position > m_size 则 m_size = m_position
    void writeStringF64(const std::string& value);

    /// 写入std::string类型的数据,用无符号Varint64作为长度类型，m_position += Varint64长度 + value.size()，如果m_position > m_size 则 m_size = m_position
    void writeStringVint(const std::string& value);

    /// 写入std::string类型的数据,无长度，m_position += value.size()，如果m_position > m_size 则 m_size = m_position
    void writeStringWithoutLength(const std::string& value);

    /// 读取int8_t类型的数据，m_position += sizeof(int8_t)，
    /// 如果m_position > m_size 则 m_size = m_position,如果getReadSize() < sizeof(int8_t) 抛出 std::out_of_range
    int8_t   readFint8();

    /// 读取uint8_t类型的数据，m_position += sizeof(uint8_t)
    uint8_t  readFuint8();

    /// 读取int16_t类型的数据，m_position += sizeof(int16_t)
    int16_t  readFint16();

    /// 读取uint16_t类型的数据，m_position += sizeof(uint16_t)
    uint16_t readFuint16();

    /// 读取int32_t类型的数据，m_position += sizeof(int32_t)
    int32_t  readFint32();

    /// 读取uint32_t类型的数据，m_position += sizeof(uint32_t)
    uint32_t readFuint32();

    /// 读取int64_t类型的数据，m_position += sizeof(int64_t)
    int64_t  readFint64();

    /// 读取uint64_t类型的数据，m_position += sizeof(uint64_t)
    uint64_t readFuint64();

    /// 读取有符号Varint32类型的数据，如果getReadSize() >= 有符号Varint32实际占用内存，m_position += 有符号Varint32实际占用内存
    /// 如果getReadSize() < 有符号Varint32实际占用内存 抛出 std::out_of_range
    int32_t  readInt32();

    /// 读取无符号Varint32类型的数据，如果getReadSize() >= 无符号Varint32实际占用内存，m_position += 无符号Varint32实际占用内存
    /// 如果getReadSize() < 无符号Varint32实际占用内存 抛出 std::out_of_range
    uint32_t readUint32();

    /// 读取有符号Varint64类型的数据，如果getReadSize() >= 有符号Varint64实际占用内存，m_position += 有符号Varint64实际占用内存
    /// 如果getReadSize() < 有符号Varint64实际占用内存 抛出 std::out_of_range
    int64_t  readInt64();

    /// 读取无符号Varint64类型的数据，如果getReadSize() >= 无符号Varint64实际占用内存，m_position += 无符号Varint64实际占用内存
    /// 如果getReadSize() < 无符号Varint64实际占用内存 抛出 std::out_of_range
    uint64_t readUint64();

    /// 读取float类型的数据，如果getReadSize() >= sizeof(float)，m_position += sizeof(float)
    /// 如果getReadSize() < sizeof(float) 抛出 std::out_of_range
    float readFloat();

    /// 读取double类型的数据，如果getReadSize() >= sizeof(double)，m_position += sizeof(double)
    /// 如果getReadSize() < sizeof(double) 抛出 std::out_of_range
    double readDouble();

    /// 读取std::string类型的数据,用uint16_t作为长度，如果getReadSize() >= sizeof(uint16_t) + size，m_position += sizeof(uint16_t) + size
    /// 如果getReadSize() < sizeof(uint16_t) + size 抛出 std::out_of_range
    std::string readStringF16();

    /// 读取std::string类型的数据,用uint32_t作为长度，如果getReadSize() >= sizeof(uint32_t) + size，m_position += sizeof(uint32_t) + size
    /// 如果getReadSize() < sizeof(uint32_t) + size 抛出 std::out_of_range
    std::string readStringF32();

    /// 读取std::string类型的数据,用uint64_t作为长度，如果getReadSize() >= sizeof(uint64_t) + size，m_position += sizeof(uint64_t) + size
    /// 如果getReadSize() < sizeof(uint64_t) + size 抛出 std::out_of_range
    std::string readStringF64();

    /// 读取std::string类型的数据,用无符号Varint64作为长度，如果getReadSize() >= 无符号Varint64实际大小 + size，m_position += 无符号Varint64实际大小 + size
    /// 如果getReadSize() < 无符号Varint64实际大小 + size 抛出 std::out_of_range
    std::string readStringVint();

    /// 清空ByteArray，m_position = 0, m_size = 0
    void clear();

    /// 写入size长度的数据， buf 内存缓存指针， size 数据大小
    void write(const void* buf, size_t size);

    /// 读取size长度的数据， buf 内存缓存指针， size 数据大小
    void read(void* buf, size_t size);

    /// 读取size长度的数据， buf 内存缓存指针， size 数据大小， position 读取开始位置
    void read(void* buf, size_t size, size_t position) const;

    /// 返回ByteArray当前位置
    size_t getPosition() const { return m_position;}

    /// 设置ByteArray当前位置，如果m_position > m_size 则 m_size = m_position
    /// 如果m_position > m_capacity 则抛出 std::out_of_range
    void setPosition(size_t v);

    /// 把ByteArray的数据写入到文件中，name 文件名
    bool writeToFile(const std::string& name) const;

    /// 从文件中读取数据，name 文件名
    bool readFromFile(const std::string& name);

    /// 返回内存块的大小
    size_t getBaseSize() const { return m_baseSize;}

    /// 返回可读取数据大小
    size_t getReadSize() const { return m_size - m_position;}

    /// 是否是小端
    bool isLittleEndian() const;

    /// 设置是否为小端
    void setIsLittleEndian(bool val);

    /// 将ByteArray里面的数据[m_position, m_size)转成std::string
    std::string toString() const;

    /// 将ByteArray里面的数据[m_position, m_size)转成16进制的std::string(格式:FF FF FF)
    std::string toHexString() const;

    /**
     * @brief 获取可读取的缓存,保存成iovec数组
     * @param[out] buffers 保存可读取数据的iovec数组
     * @param[in] len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
     * @return 返回实际数据的长度
     */
    /// 获取可读取的缓存,保存成iovec数组, 返回实际数据的长度
    /// len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;

    /// 获取可读取的缓存,保存成iovec数组,从position位置开始, 返回实际数据的长度
    /// buffers 保存可读取数据的iovec数组，position 读取数据的位置
    /// len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;


    /// 获取可写入的缓存,保存成iovec数组,返回实际的长度
    /// buffers 保存可写入的内存的iovec数组，len 写入的长度
    /// 如果(m_position + len) > m_capacity 则 m_capacity扩容N个节点以容纳len长度
    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    /// 返回数据的长度
    size_t getSize() const { return m_size;}
private:
    
    /// 扩容ByteArray,使其可以容纳size个数据(如果原本可以可以容纳,则不扩容)
    void addCapacity(size_t size);

    /// 返回当前的可写入容量
    size_t getCapacity() const { return m_capacity - m_position;}
private:
    
    size_t m_baseSize;      // 内存块的大小
    size_t m_position;      // 当前操作位置
    size_t m_capacity;      // 当前的总容量
    size_t m_size;          // 当前数据的大小
    int8_t m_endian;        // 字节序,默认大端
    Node* m_root;           // 第一个内存块指针
    Node* m_cur;            // 当前操作的内存块指针
};

}

#endif