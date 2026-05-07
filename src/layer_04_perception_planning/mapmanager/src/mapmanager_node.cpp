#include "mapmanager/mapmanager_node.hpp"

namespace mapmanager
{

MapManagerNode::MapManagerNode(const rclcpp::NodeOptions &options)
: Node("mapmanager_node", options)
{
  RCLCPP_INFO(this->get_logger(), "MapManagerNode initialized");
}

MapManagerNode::~MapManagerNode()
{
  RCLCPP_INFO(this->get_logger(), "MapManagerNode shutting down");
}

} // namespace mapmanager

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(mapmanager::MapManagerNode)
