cmake_minimum_required(VERSION 3.8)
project(uc_common)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(mc_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(Eigen3 REQUIRED)

# UC plugin shared library
add_library(uc_plugin SHARED
  src/uc_plugin.cpp
  src/ik_solver.cpp
  src/trajectory_planner.cpp
  src/force_controller.cpp
  src/gripper_controller.cpp
  src/self_collision_checker.cpp
  src/base_compensator.cpp
)

target_include_directories(uc_plugin PUBLIC
  <BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  <INSTALL_INTERFACE:include>
  ${EIGEN3_INCLUDE_DIRS}
)

target_compile_features(uc_plugin PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(uc_plugin
  rclcpp
  mc_msgs
  geometry_msgs
  tf2
  tf2_geometry_msgs
  Eigen3
)

install(TARGETS uc_plugin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_libraries(uc_plugin)
ament_package()
