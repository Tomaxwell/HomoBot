cmake_minimum_required(VERSION 3.8)
project(agent)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(agent_msgs REQUIRED)
find_package(rclcpp_action REQUIRED)

add_executable(agent_node
  src/agent_node.cpp
)

target_include_directories(agent_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(agent_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(agent_node
  rclcpp
  agent_msgs
  rclcpp_action
)

install(TARGETS agent_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
