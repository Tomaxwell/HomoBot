cmake_minimum_required(VERSION 3.8)
project(ms_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)

set(msg_files
  "msg/TeleopState.msg"
  "msg/DeviceState.msg"
  "msg/RetargetingConfig.msg"
  "msg/Heartbeat.msg"
)

set(srv_files
  "srv/StartTeleop.srv"
  "srv/StopTeleop.srv"
  "srv/PauseTeleop.srv"
  "srv/GetDeviceStatus.srv"
  "srv/GetHealthStatus.srv"
)

set(action_files
  "action/ExecuteTeleop.action"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  ${action_files}
  DEPENDENCIES builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
