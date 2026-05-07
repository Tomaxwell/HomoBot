cmake_minimum_required(VERSION 3.8)
project(dr_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(sensor_msgs REQUIRED)

set(msg_files
  "msg/RecorderState.msg"
  "msg/RecordingSession.msg"
  "msg/VlaDataFrame.msg"
  "msg/FailureContext.msg"
  "msg/DataAssetMetadata.msg"
  "msg/Heartbeat.msg"
)

set(srv_files
  "srv/GetHealthStatus.srv"
  "srv/StartRecording.srv"
  "srv/StopRecording.srv"
  "srv/ListRecordings.srv"
  "srv/ExportRecording.srv"
  "srv/DeleteRecording.srv"
  "srv/TagRecording.srv"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  DEPENDENCIES builtin_interfaces sensor_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
