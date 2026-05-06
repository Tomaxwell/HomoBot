cmake_minimum_required(VERSION 3.8)
project(te)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(te_msgs REQUIRED)
find_package(rclcpp_action REQUIRED)

add_executable(te_node
  src/te_node.cpp
)

target_include_directories(te_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(te_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(te_node
  rclcpp
  te_msgs
  rclcpp_action
)

install(TARGETS te_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
