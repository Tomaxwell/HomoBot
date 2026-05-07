cmake_minimum_required(VERSION 3.8)
project(lc_wheeled)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(mc_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(Eigen3 REQUIRED)

# LC wheeled plugin shared library
add_library(lc_wheeled_plugin SHARED
  src/lc_plugin.cpp
  src/chassis_controller.cpp
  src/lift_controller.cpp
  src/caster_monitor.cpp
)

target_include_directories(lc_wheeled_plugin PUBLIC
  <BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  <INSTALL_INTERFACE:include>
  ${EIGEN3_INCLUDE_DIRS}
)

target_compile_features(lc_wheeled_plugin PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(lc_wheeled_plugin
  rclcpp
  mc_msgs
  geometry_msgs
  Eigen3
)

install(TARGETS lc_wheeled_plugin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_libraries(lc_wheeled_plugin)
ament_package()
