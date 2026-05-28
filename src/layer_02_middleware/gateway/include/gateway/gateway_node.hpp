#ifndef GATEWAY__GATEWAYNODE_HPP_
#define GATEWAY__GATEWAYNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace gateway
{

class GatewayNode : public rclcpp::Node
{
public:
  explicit GatewayNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~GatewayNode() override;

private:
};

} // namespace gateway

#endif // GATEWAY__GATEWAYNODE_HPP_
