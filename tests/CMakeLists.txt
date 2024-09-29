file(GLOB TEST_SRC "*.cpp")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 添加测试文件
foreach(testfile ${TEST_SRC})
    # 获取文件名， 
    # 对于 test_log.cpp，testname 将被设置为 test_log
    get_filename_component(testname ${testfile} NAME_WE)
    sylar_add_executable(${testname} ${testfile} sylar "${LIBS}")
endforeach()

