#ifndef VSLAM__VSLAMNODE_HPP_
#define VSLAM__VSLAMNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace vslam
{

class VslamNode : public rclcpp::Node
{
public:
  explicit VslamNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~VslamNode() override;

private:
};

} // namespace vslam

#endif // VSLAM__VSLAMNODE_HPP_
