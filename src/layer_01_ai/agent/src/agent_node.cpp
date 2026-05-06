#include "agent/agent_node.hpp"

namespace agent
{

AgentNode::AgentNode(const rclcpp::NodeOptions &options)
: Node("agent_node", options)
{
  RCLCPP_INFO(this->get_logger(), "AgentNode initialized");
}

AgentNode::~AgentNode()
{
  RCLCPP_INFO(this->get_logger(), "AgentNode shutting down");
}

} // namespace agent

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(agent::AgentNode)
