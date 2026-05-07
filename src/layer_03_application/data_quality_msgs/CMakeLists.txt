cmake_minimum_required(VERSION 3.8)
project(data_quality_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)

set(msg_files
  "msg/DataQualityScore.msg"
  "msg/FrameQualityReport.msg"
  "msg/RedundancyFlag.msg"
  "msg/Heartbeat.msg"
)

set(srv_files
  "srv/EvaluateDataQuality.srv"
  "srv/GetQualityThresholds.srv"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  DEPENDENCIES builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
