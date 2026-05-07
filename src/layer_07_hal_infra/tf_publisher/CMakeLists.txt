cmake_minimum_required(VERSION 3.8)
project(tf_publisher)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(urdfdom_headers REQUIRED)
find_package(tf_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

add_executable(tf_publisher_node
  src/tf_publisher_node.cpp
  src/model_manager.cpp
  src/transform_builder.cpp
  src/frame_graph_monitor.cpp
)

target_include_directories(tf_publisher_node PUBLIC
  <BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  <INSTALL_INTERFACE:include>
)

target_compile_features(tf_publisher_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(tf_publisher_node
  rclcpp
  tf2
  tf2_ros
  urdfdom_headers
  tf_msgs
  geometry_msgs
)

install(TARGETS tf_publisher_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
