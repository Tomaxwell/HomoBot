#include "lidar_slam/lidar_slam_node.hpp"

namespace lidar_slam
{

LidarSlamNode::LidarSlamNode(const rclcpp::NodeOptions &options)
: Node("lidar_slam_node", options)
{
  RCLCPP_INFO(this->get_logger(), "LidarSlamNode initialized");
}

LidarSlamNode::~LidarSlamNode()
{
  RCLCPP_INFO(this->get_logger(), "LidarSlamNode shutting down");
}

} // namespace lidar_slam

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(lidar_slam::LidarSlamNode)
