#ifndef HAL__ETHERCAT__HALETHERCATNODE_HPP_
#define HAL__ETHERCAT__HALETHERCATNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace hal_ethercat
{

class HalEthercatNode : public rclcpp::Node
{
public:
  explicit HalEthercatNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~HalEthercatNode() override;

private:
};

} // namespace hal_ethercat

#endif // HAL__ETHERCAT__HALETHERCATNODE_HPP_
