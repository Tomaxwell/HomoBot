cmake_minimum_required(VERSION 3.8)
project(data_rule_engine)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(data_rule_msgs REQUIRED)
find_package(sm_msgs REQUIRED)
find_package(hds_msgs REQUIRED)

add_executable(data_rule_engine_node
  src/data_rule_engine_node.cpp
)

target_include_directories(data_rule_engine_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(data_rule_engine_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(data_rule_engine_node
  rclcpp
  std_msgs
  data_rule_msgs
  sm_msgs
  hds_msgs
)

install(TARGETS data_rule_engine_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
