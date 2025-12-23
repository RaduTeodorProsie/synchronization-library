cmake_minimum_required(VERSION 3.30)
project(synchronization)

set(CMAKE_CXX_STANDARD 23)

add_executable(synchronization main.cpp SpinLock.cpp)
