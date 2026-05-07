#include "perception/perception_node.hpp"

namespace perception
{

PerceptionNode::PerceptionNode(const rclcpp::NodeOptions &options)
: Node("perception_node", options)
{
  RCLCPP_INFO(this->get_logger(), "PerceptionNode initialized");
}

PerceptionNode::~PerceptionNode()
{
  RCLCPP_INFO(this->get_logger(), "PerceptionNode shutting down");
}

} // namespace perception

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(perception::PerceptionNode)
