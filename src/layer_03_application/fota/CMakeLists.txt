cmake_minimum_required(VERSION 3.8)
project(fota)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(fota_msgs REQUIRED)

add_executable(fota_node
  src/fota_node.cpp
)

target_include_directories(fota_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(fota_node PUBLIC c_std_99 cxx_std_17)

ament_target_dependencies(fota_node
  rclcpp
  fota_msgs
)

install(TARGETS fota_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
