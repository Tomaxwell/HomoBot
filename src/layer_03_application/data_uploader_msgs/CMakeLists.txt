cmake_minimum_required(VERSION 3.8)
project(data_uploader_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(action_msgs REQUIRED)

set(msg_files
  "msg/UploadStatus.msg"
  "msg/DataAssetInfo.msg"
  "msg/UploadPriority.msg"
  "msg/BandwidthStatus.msg"
  "msg/Heartbeat.msg"
)

set(srv_files
  "srv/TriggerUpload.srv"
  "srv/GetUploadQueue.srv"
  "srv/CancelUpload.srv"
)

set(action_files
  "action/UploadDataset.action"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${srv_files}
  ${action_files}
  DEPENDENCIES builtin_interfaces action_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
