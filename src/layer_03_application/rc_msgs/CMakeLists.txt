cmake_minimum_required(VERSION 3.8)
project(rc_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)

set(msg_files
  "msg/SystemMetrics.msg"
  "msg/ModuleHealth.msg"
  "msg/SystemEvent.msg"
  "msg/LogEntry.msg"
  "msg/Heartbeat.msg"
)

set(srv_files
  "srv/GetHealthStatus.srv"
  "srv/QueryEvents.srv"
  "srv/QueryLogs.srv"
  "srv/QueryMetrics.srv"
  "srv/GetSystemSummary.srv"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  DEPENDENCIES builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
