#ifndef PERCEPTION__PERCEPTIONNODE_HPP_
#define PERCEPTION__PERCEPTIONNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace perception
{

class PerceptionNode : public rclcpp::Node
{
public:
  explicit PerceptionNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~PerceptionNode() override;

private:
};

} // namespace perception

#endif // PERCEPTION__PERCEPTIONNODE_HPP_
