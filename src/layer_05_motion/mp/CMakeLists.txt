cmake_minimum_required(VERSION 3.8)
project(mp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(mp_msgs REQUIRED)
find_package(mc_msgs REQUIRED)
find_package(sm_msgs REQUIRED)

add_executable(mp_node
  src/mp_node.cpp
)

target_include_directories(mp_node PUBLIC
  <BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  <INSTALL_INTERFACE:include>
)

target_compile_features(mp_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(mp_node
  rclcpp
  rclcpp_action
  std_msgs
  geometry_msgs
  mp_msgs
  mc_msgs
  sm_msgs
)

install(TARGETS mp_node
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
