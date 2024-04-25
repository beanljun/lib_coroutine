

#ifndef __SYLAR_NONCOPYABLE_H__
#define __SYLAR_NONCOPYABLE_H__

namespace sylar {
/**
 * @brief 继承该类可以禁止拷贝构造和赋值操作
 */
class Noncopyable {
public:
    /// @brief 默认构造函数
    Noncopyable() = default;

    /// @brief 默认析构函数
    ~Noncopyable() = default;

    /// @brief  禁止拷贝
    //C++11新特性，表示将拷贝构造函数设置为删除状态。
    //不能用已存在的对象来初始化新对象，也不能将一个对象赋值给另一个对象。
    Noncopyable(const Noncopyable&) = delete;

    /// @brief 禁止赋值操作，不能将一个对象赋值给另一个对象。
    Noncopyable& operator=(const Noncopyable&) = delete;

};

}

#endif