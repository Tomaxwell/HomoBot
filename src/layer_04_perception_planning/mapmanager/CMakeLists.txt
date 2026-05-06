cmake_minimum_required(VERSION 3.8)
project(mapmanager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(mapmanager_msgs REQUIRED)

add_executable(mapmanager_node
  src/mapmanager_node.cpp
)

target_include_directories(mapmanager_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(mapmanager_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(mapmanager_node
  rclcpp
  mapmanager_msgs
)

install(TARGETS mapmanager_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
