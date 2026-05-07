cmake_minimum_required(VERSION 3.8)
project(ms_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)

set(msg_files
  "msg/MotionStreamerState.msg"
  "msg/MotionCommand.msg"
  "msg/MotionTarget.msg"
  "msg/StreamStatistics.msg"
  "msg/Heartbeat.msg"
)

set(srv_files
  "srv/GetHealthStatus.srv"
  "srv/SetStreamParameters.srv"
  "srv/StopStream.srv"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  DEPENDENCIES builtin_interfaces geometry_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
