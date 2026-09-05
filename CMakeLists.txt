cmake_minimum_required(VERSION 3.30)
project(synchronization)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(spinlock_lib 
    src/SpinLock.cpp src/SpinLock.h 
    src/LockGuard.h 
    src/RwLock.h src/RwLock.cpp 
    src/Mutex.h src/Mutex.cpp 
    src/TicketLock.h src/TicketLock.cpp
    src/SeqLock.h
    src/RingBuffer.h
    src/Backoff.h
    src/HazardPointers.h
    src/TreiberStack.h
)
target_include_directories(spinlock_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)



# Tests
enable_testing()
include(GoogleTest)
add_executable(unit_tests tests/synchronization_tests.cpp)
target_link_libraries(unit_tests PRIVATE spinlock_lib gtest_main)
gtest_discover_tests(unit_tests)

