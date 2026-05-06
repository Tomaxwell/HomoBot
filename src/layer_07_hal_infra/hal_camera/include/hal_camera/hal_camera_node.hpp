#ifndef HAL_CAMERA__HALCAMERANODE_HPP_
#define HAL_CAMERA__HALCAMERANODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace hal_camera
{

class HalCameraNode : public rclcpp::Node
{
public:
  explicit HalCameraNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~HalCameraNode() override;

private:
};

} // namespace hal_camera

#endif // HAL_CAMERA__HALCAMERANODE_HPP_
