cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)

if(ANDROID)
  find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
  find_package(cmake-android REQUIRED PATHS node_modules/cmake-android)
endif()

project(bare_android_wifi_manager C CXX)

if(ANDROID)
  fetch_package("github:holepunchto/libjnitl")
endif()

add_bare_module(bare_android_wifi_manager)

target_sources(
  ${bare_android_wifi_manager}
  PRIVATE
    binding.cc
)

if(ANDROID)
  target_link_libraries(
    ${bare_android_wifi_manager}
    PRIVATE
      jnitl
      log
  )
endif()

set_target_properties(
  ${bare_android_wifi_manager}
  PROPERTIES
  C_STANDARD 11
  CXX_STANDARD 20
  CXX_SCAN_FOR_MODULES OFF
  POSITION_INDEPENDENT_CODE ON
)
