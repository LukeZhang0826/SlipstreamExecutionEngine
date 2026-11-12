cmake_minimum_required(VERSION 4.4)
project(SlipstreamExecutionEngine CXX)

set(CMAKE_CXX_STANDARD 23) # version
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic) # before any target applies those flags to everything decalred after it

add_library(slipstream_codec src/codec.cpp
        include/slipstream/frame_reader.h
        tests/codec_test.cpp
        src/net.cpp
        include/slipstream/net.h
)
target_include_directories(slipstream_codec PUBLIC include) # adds include to header search path

add_executable(SlipstreamEE src/main.cpp)
target_link_libraries(SlipstreamEE PRIVATE slipstream_codec) # makes exec depend on the library and link it in PRIVATE because nothing links against the exec

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)