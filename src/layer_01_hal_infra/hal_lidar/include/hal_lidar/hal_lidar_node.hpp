#ifndef HAL_LIDAR__HALLIDARNODE_HPP_
#define HAL_LIDAR__HALLIDARNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace hal_lidar
{

class HalLidarNode : public rclcpp::Node
{
public:
  explicit HalLidarNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~HalLidarNode() override;

private:
};

} // namespace hal_lidar

#endif // HAL_LIDAR__HALLIDARNODE_HPP_
