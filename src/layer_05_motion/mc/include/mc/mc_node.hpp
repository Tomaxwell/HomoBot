#ifndef MC__MOTIONCONTROLNODE_HPP_
#define MC__MOTIONCONTROLNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace mc
{

class MotionControlNode : public rclcpp::Node
{
public:
  explicit MotionControlNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~MotionControlNode() override;

private:
};

} // namespace mc

#endif // MC__MOTIONCONTROLNODE_HPP_
