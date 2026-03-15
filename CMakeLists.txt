cmake_minimum_required(VERSION 3.10)

project(kvstore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 如果编译时没指定类型，默认给它Release(最高性能)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# 打印当前构建类型，方便确认
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 查找系统的多线程库
find_package(Threads REQUIRED)

set(MY_LEVELDB_ROOT "/home/apirlstorm/leveldb")

include_directories(
    ${PROJECT_SOURCE_DIR}/include  # 你项目的自定义头文件路径
    ${MY_LEVELDB_ROOT}/include
)

link_directories(
    /usr/local/lib
    ${MY_LEVELDB_ROOT}/build
)

add_executable(tcptest
    test/tcpTest.cpp 
    src/EventLoop.cpp 
    src/EventLoopThread.cpp 
    src/EventLoopThreadPool.cpp 
    src/TcpServer.cpp 
    src/Connection.cpp
    src/KVProtocol.cpp
    src/HashLRUCache.cpp
    src/LRUCache.cpp
    src/KVService.cpp
    src/LevelDBStore.cpp
    src/TaskQueue.cpp
    src/ThreadPool.cpp
)

target_include_directories(tcptest PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/utils
)

target_link_libraries(tcptest 
    pthread 
    event 
    event_pthreads 
    ${MY_LEVELDB_ROOT}/build/libleveldb.a
)
