#include "hal_ethercat/hal_ethercat_node.hpp"

namespace hal_ethercat
{

HalEthercatNode::HalEthercatNode(const rclcpp::NodeOptions &options)
: Node("hal_ethercat_node", options)
{
  RCLCPP_INFO(this->get_logger(), "HalEthercatNode initialized");
}

HalEthercatNode::~HalEthercatNode()
{
  RCLCPP_INFO(this->get_logger(), "HalEthercatNode shutting down");
}

} // namespace hal_ethercat

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(hal_ethercat::HalEthercatNode)
