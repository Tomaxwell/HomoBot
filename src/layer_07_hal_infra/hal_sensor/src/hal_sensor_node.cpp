#include "hal_sensor/hal_sensor_node.hpp"

namespace hal_sensor
{

HalSensorNode::HalSensorNode(const rclcpp::NodeOptions &options)
: Node("hal_sensor_node", options)
{
  RCLCPP_INFO(this->get_logger(), "HalSensorNode initialized");
}

HalSensorNode::~HalSensorNode()
{
  RCLCPP_INFO(this->get_logger(), "HalSensorNode shutting down");
}

} // namespace hal_sensor

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(hal_sensor::HalSensorNode)
