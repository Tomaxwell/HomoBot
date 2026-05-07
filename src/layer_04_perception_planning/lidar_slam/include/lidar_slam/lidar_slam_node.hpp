#ifndef LIDAR_SLAM__LIDARSLAMNODE_HPP_
#define LIDAR_SLAM__LIDARSLAMNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace lidar_slam
{

class LidarSlamNode : public rclcpp::Node
{
public:
  explicit LidarSlamNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~LidarSlamNode() override;

private:
};

} // namespace lidar_slam

#endif // LIDAR_SLAM__LIDARSLAMNODE_HPP_
