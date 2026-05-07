#ifndef HDS__HEALTHDIAGNOSISNODE_HPP_
#define HDS__HEALTHDIAGNOSISNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace hds
{

class HealthDiagnosisNode : public rclcpp::Node
{
public:
  explicit HealthDiagnosisNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~HealthDiagnosisNode() override;

private:
};

} // namespace hds

#endif // HDS__HEALTHDIAGNOSISNODE_HPP_
