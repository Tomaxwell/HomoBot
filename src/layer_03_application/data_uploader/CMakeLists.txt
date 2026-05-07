cmake_minimum_required(VERSION 3.8)
project(data_uploader)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(data_uploader_msgs REQUIRED)
find_package(dr_msgs REQUIRED)
find_package(hds_msgs REQUIRED)

add_executable(data_uploader_node
  src/data_uploader_node.cpp
)

target_include_directories(data_uploader_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(data_uploader_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(data_uploader_node
  rclcpp
  rclcpp_action
  std_msgs
  data_uploader_msgs
  dr_msgs
  hds_msgs
)

install(TARGETS data_uploader_node
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
