#include "te/te_node.hpp"

namespace te
{

TaskEngineNode::TaskEngineNode(const rclcpp::NodeOptions &options)
: Node("te_node", options)
{
  RCLCPP_INFO(this->get_logger(), "TaskEngineNode initialized");
}

TaskEngineNode::~TaskEngineNode()
{
  RCLCPP_INFO(this->get_logger(), "TaskEngineNode shutting down");
}

} // namespace te

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(te::TaskEngineNode)
