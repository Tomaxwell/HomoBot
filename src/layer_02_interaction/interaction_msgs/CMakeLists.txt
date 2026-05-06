cmake_minimum_required(VERSION 3.8)
project(interaction_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)

set(msg_files
  "msg/InteractionState.msg"
  "msg/InteractionEvent.msg"
  "msg/VoiceIntent.msg"
  "msg/VisualIntent.msg"
  "msg/TouchIntent.msg"
  "msg/MessageIntent.msg"
  "msg/EmotionState.msg"
  "msg/Heartbeat.msg"
)

set(srv_files
  "srv/GetHealthStatus.srv"
  "srv/QueryInteractionHistory.srv"
  "srv/SetInteractionMode.srv"
  "srv/SendMessage.srv"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  DEPENDENCIES builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
