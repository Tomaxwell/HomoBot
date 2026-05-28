cmake_minimum_required(VERSION 3.8)
project(setting)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(setting_msgs REQUIRED)

add_executable(setting_node
  src/setting_node.cpp
)

target_include_directories(setting_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(setting_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(setting_node
  rclcpp
  setting_msgs
)

install(TARGETS setting_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
