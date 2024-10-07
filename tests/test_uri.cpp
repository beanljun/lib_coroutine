
#include <iostream>

#include "../sylar/sylar.h"

int main(int argc, char* argv[]) {
    auto uri = sylar::Uri::Create("http://a:b@host.com:8080/p/a/t/h?query=string#hash");
    std::cout << uri->toString() << std::endl;
    return 0;
}