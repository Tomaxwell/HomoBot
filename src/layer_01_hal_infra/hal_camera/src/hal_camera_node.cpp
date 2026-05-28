#include "hal_camera/hal_camera_node.hpp"

namespace hal_camera
{

HalCameraNode::HalCameraNode(const rclcpp::NodeOptions &options)
: Node("hal_camera_node", options)
{
  RCLCPP_INFO(this->get_logger(), "HalCameraNode initialized");
}

HalCameraNode::~HalCameraNode()
{
  RCLCPP_INFO(this->get_logger(), "HalCameraNode shutting down");
}

} // namespace hal_camera

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(hal_camera::HalCameraNode)
