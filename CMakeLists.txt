cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)

if(ANDROID)
  find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
  find_package(cmake-android REQUIRED PATHS node_modules/cmake-android)
endif()

project(bare_wifi_android C CXX)

if(ANDROID)
  fetch_package("github:holepunchto/libjnitl")
endif()

add_bare_module(bare_wifi_android)

target_sources(
  ${bare_wifi_android}
  PRIVATE
    binding.cc
)

if(ANDROID)
  target_link_libraries(
    ${bare_wifi_android}
    PRIVATE
      jnitl
      log
  )
endif()

set_target_properties(
  ${bare_wifi_android}
  PROPERTIES
  C_STANDARD 11
  CXX_STANDARD 20
  CXX_SCAN_FOR_MODULES OFF
  POSITION_INDEPENDENT_CODE ON
)
