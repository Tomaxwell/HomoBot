cmake_minimum_required(VERSION 3.8)
project(interaction)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(interaction_msgs REQUIRED)
find_package(hal_audio_msgs REQUIRED)
find_package(sm_msgs REQUIRED)
find_package(te_msgs REQUIRED)
find_package(mp_msgs REQUIRED)

add_executable(interaction_node
  src/interaction_node.cpp
)

target_include_directories(interaction_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(interaction_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(interaction_node
  rclcpp
  std_msgs
  interaction_msgs
  hal_audio_msgs
  sm_msgs
  te_msgs
  mp_msgs
)

install(TARGETS interaction_node
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
