#include "hal_lidar/hal_lidar_node.hpp"

namespace hal_lidar
{

HalLidarNode::HalLidarNode(const rclcpp::NodeOptions &options)
: Node("hal_lidar_node", options)
{
  RCLCPP_INFO(this->get_logger(), "HalLidarNode initialized");
}

HalLidarNode::~HalLidarNode()
{
  RCLCPP_INFO(this->get_logger(), "HalLidarNode shutting down");
}

} // namespace hal_lidar

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(hal_lidar::HalLidarNode)
