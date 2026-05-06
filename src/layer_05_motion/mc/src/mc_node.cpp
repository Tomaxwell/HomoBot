#include "mc/mc_node.hpp"

namespace mc
{

MotionControlNode::MotionControlNode(const rclcpp::NodeOptions &options)
: Node("mc_node", options)
{
  RCLCPP_INFO(this->get_logger(), "MotionControlNode initialized");
}

MotionControlNode::~MotionControlNode()
{
  RCLCPP_INFO(this->get_logger(), "MotionControlNode shutting down");
}

} // namespace mc

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(mc::MotionControlNode)
