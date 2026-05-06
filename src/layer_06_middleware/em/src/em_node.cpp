#include "em/em_node.hpp"

namespace em
{

ExecutiveManagerNode::ExecutiveManagerNode(const rclcpp::NodeOptions &options)
: Node("em_node", options)
{
  RCLCPP_INFO(this->get_logger(), "ExecutiveManagerNode initialized");
}

ExecutiveManagerNode::~ExecutiveManagerNode()
{
  RCLCPP_INFO(this->get_logger(), "ExecutiveManagerNode shutting down");
}

} // namespace em

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(em::ExecutiveManagerNode)
