project(metronoom)
cmake_minimum_required(VERSION 3.2)
add_definitions("-std=gnu++17 -Wall -Werror")

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fstack-protector-strong")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer")

add_executable(
  metronoom
  metronoom.cpp
  mdns_rtpmidi.cpp
)

#target_link_libraries(metronoom fmt rtpmididstatic)
target_link_libraries(metronoom fmt rtpmidid fmt)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(metronoom Threads::Threads)

include(FindPkgConfig)

pkg_check_modules(AVAHI REQUIRED avahi-client)
target_link_libraries(metronoom ${AVAHI_LIBRARIES})
target_include_directories(metronoom PUBLIC ${AVAHI_INCLUDE_DIRS})
target_compile_options(metronoom PUBLIC ${AVAHI_CFLAGS_OTHER})

pkg_check_modules(FMT REQUIRED fmt)
target_link_libraries(metronoom ${FMT_LIBRARIES})
target_include_directories(metronoom PUBLIC ${FMT_INCLUDE_DIRS})
target_compile_options(metronoom PUBLIC ${FMT_CFLAGS_OTHER})

set_target_properties(metronoom PROPERTIES OUTPUT_NAME metronoom)

target_link_libraries(metronoom rt)
