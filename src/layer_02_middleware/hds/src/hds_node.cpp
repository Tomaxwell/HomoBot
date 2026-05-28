#include "hds/hds_node.hpp"

namespace hds
{

HealthDiagnosisNode::HealthDiagnosisNode(const rclcpp::NodeOptions &options)
: Node("hds_node", options)
{
  RCLCPP_INFO(this->get_logger(), "HealthDiagnosisNode initialized");
}

HealthDiagnosisNode::~HealthDiagnosisNode()
{
  RCLCPP_INFO(this->get_logger(), "HealthDiagnosisNode shutting down");
}

} // namespace hds

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(hds::HealthDiagnosisNode)
