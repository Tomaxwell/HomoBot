cmake_minimum_required(VERSION 3.8)
project(hal_audio)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(hal_audio_msgs REQUIRED)
find_package(sm_msgs REQUIRED)

add_executable(hal_audio_node
  src/hal_audio_node.cpp
)

target_include_directories(hal_audio_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(hal_audio_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(hal_audio_node
  rclcpp
  rclcpp_action
  std_msgs
  hal_audio_msgs
  sm_msgs
)

install(TARGETS hal_audio_node
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
