#include "fota/fota_node.hpp"

namespace fota
{

FotaNode::FotaNode(const rclcpp::NodeOptions &options)
: Node("fota_node", options)
{
  RCLCPP_INFO(this->get_logger(), "FotaNode initialized");
}

FotaNode::~FotaNode()
{
  RCLCPP_INFO(this->get_logger(), "FotaNode shutting down");
}

} // namespace fota

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(fota::FotaNode)
