#include "pnc/pnc_node.hpp"

namespace pnc
{

PncNode::PncNode(const rclcpp::NodeOptions &options)
: Node("pnc_node", options)
{
  RCLCPP_INFO(this->get_logger(), "PncNode initialized");
}

PncNode::~PncNode()
{
  RCLCPP_INFO(this->get_logger(), "PncNode shutting down");
}

} // namespace pnc

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(pnc::PncNode)
