cmake_minimum_required(VERSION 3.8)
project(sm_msgs)

find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)

set(msg_files
  "msg/RobotState.msg"
  "msg/TransitionEvent.msg"
  "msg/Heartbeat.msg"
  "msg/ErrorCode.msg"
)

set(srv_files
  "srv/RequestTransition.srv"
  "srv/GetState.srv"
  "srv/AcknowledgeFault.srv"
  "srv/IsMotionAllowed.srv"
  "srv/GetHealthStatus.srv"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  DEPENDENCIES builtin_interfaces geometry_msgs sensor_msgs nav_msgs std_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
