#ifndef TF_PUBLISHER__TF_PUBLISHER_NODE_HPP_
#define TF_PUBLISHER__TF_PUBLISHER_NODE_HPP_

#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/transform_broadcaster.h"
#include "tf2_ros/static_transform_broadcaster.h"

// Forward declarations
namespace tf_publisher
{
class ModelManager;
class TransformBuilder;
class FrameGraphMonitor;

/**
 * @brief TF Publisher node
 *
 * Parses URDF robot description and publishes /tf and /tf_static
 * transforms. Monitors frame graph health.
 */
class TFPublisherNode : public rclcpp::Node
{
public:
  explicit TFPublisherNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~TFPublisherNode() override;

private:
  void load_robot_description();
  void publish_transforms();
  void publish_heartbeat();
  void handle_reload_request(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<tf_msgs::srv::ReloadRobotDescription::Request> request,
    std::shared_ptr<tf_msgs::srv::ReloadRobotDescription::Response> response);

  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  std::unique_ptr<tf2_ros::StaticTransformBroadcaster> static_tf_broadcaster_;

  std::unique_ptr<ModelManager> model_manager_;
  std::unique_ptr<TransformBuilder> transform_builder_;
  std::unique_ptr<FrameGraphMonitor> frame_graph_monitor_;

  rclcpp::TimerBase::SharedPtr publish_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr robot_description_sub_;
  rclcpp::Service<tf_msgs::srv::ReloadRobotDescription>::SharedPtr reload_service_;

  std::string robot_description_;
  double publish_rate_ = 100.0;  // Hz
  bool initialized_ = false;
};

}  // namespace tf_publisher

#endif  // TF_PUBLISHER__TF_PUBLISHER_NODE_HPP_
