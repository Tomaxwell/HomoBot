#include "sm/sm_node.hpp"

namespace sm
{

StateManagerNode::StateManagerNode(const rclcpp::NodeOptions &options)
: Node("sm_node", options)
{
  RCLCPP_INFO(this->get_logger(), "StateManagerNode initialized");
}

StateManagerNode::~StateManagerNode()
{
  RCLCPP_INFO(this->get_logger(), "StateManagerNode shutting down");
}

} // namespace sm

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(sm::StateManagerNode)
