#include "vslam/vslam_node.hpp"

namespace vslam
{

VslamNode::VslamNode(const rclcpp::NodeOptions &options)
: Node("vslam_node", options)
{
  RCLCPP_INFO(this->get_logger(), "VslamNode initialized");
}

VslamNode::~VslamNode()
{
  RCLCPP_INFO(this->get_logger(), "VslamNode shutting down");
}

} // namespace vslam

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(vslam::VslamNode)
