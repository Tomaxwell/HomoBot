#include "gateway/gateway_node.hpp"

namespace gateway
{

GatewayNode::GatewayNode(const rclcpp::NodeOptions &options)
: Node("gateway_node", options)
{
  RCLCPP_INFO(this->get_logger(), "GatewayNode initialized");
}

GatewayNode::~GatewayNode()
{
  RCLCPP_INFO(this->get_logger(), "GatewayNode shutting down");
}

} // namespace gateway

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(gateway::GatewayNode)
