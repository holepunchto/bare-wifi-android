cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)

project(bare_android_wifi_manager C)

add_bare_module(bare_android_wifi_manager)

target_sources(
  ${bare_android_wifi_manager}
  PRIVATE
    binding.c
)

if(ANDROID)
  target_link_libraries(${bare_android_wifi_manager} PRIVATE log)
endif()
