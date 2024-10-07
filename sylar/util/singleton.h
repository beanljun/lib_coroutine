/**
 * @file singleton.h
 * @author beanljun
 * @brief 单例模式
 * @date 2024-04-25
 */

#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include <memory>

namespace sylar {
namespace {

template <class T, class X, int N>
T& GetInstanceX() {
    static T v;
    return v;
}

template <class T, class X, int N>
std::shared_ptr<T> GetInstanceX() {
    static std::shared_ptr<T> v(new T);
    return v;
}

}  // namespace

/**
 * @brief 单例模式封装类
 * @tparam T 类型
 * @tparam X 为了创造多个实例对应的Tag
 * @tparam N 实例编号，默认为0
 */
template <class T, class X = void, int N = 0>
class Singleton {
public:
    /**
     * @brief 返回单例
     */
    static T* GetInstance() {
        static T v;
        return &v;
    }
};

/**
 * @brief 单例模式智能指针封装类
 * @tparam T 类型
 * @tparam X 为了创造多个实例对应的Tag
 * @tparam N 实例编号，默认为0
 */
template <class T, class X = void, int N = 0>
class SingletonPtr {
public:
    /**
     * @brief 返回单例智能指针
     */
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> v(new T);
        return v;
    }
};

}  // namespace sylar

#endif
