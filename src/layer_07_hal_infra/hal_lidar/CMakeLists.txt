cmake_minimum_required(VERSION 3.8)
project(hal_lidar)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(hal_lidar_msgs REQUIRED)

add_executable(hal_lidar_node
  src/hal_lidar_node.cpp
)

target_include_directories(hal_lidar_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(hal_lidar_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(hal_lidar_node
  rclcpp
  hal_lidar_msgs
)

install(TARGETS hal_lidar_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
