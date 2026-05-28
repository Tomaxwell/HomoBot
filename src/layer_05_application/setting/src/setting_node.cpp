#include "setting/setting_node.hpp"

namespace setting
{

SettingNode::SettingNode(const rclcpp::NodeOptions &options)
: Node("setting_node", options)
{
  RCLCPP_INFO(this->get_logger(), "SettingNode initialized");
}

SettingNode::~SettingNode()
{
  RCLCPP_INFO(this->get_logger(), "SettingNode shutting down");
}

} // namespace setting

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(setting::SettingNode)
