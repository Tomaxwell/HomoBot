#ifndef HAL_SENSOR__HALSENSORNODE_HPP_
#define HAL_SENSOR__HALSENSORNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace hal_sensor
{

class HalSensorNode : public rclcpp::Node
{
public:
  explicit HalSensorNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~HalSensorNode() override;

private:
};

} // namespace hal_sensor

#endif // HAL_SENSOR__HALSENSORNODE_HPP_
