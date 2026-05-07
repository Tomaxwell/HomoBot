#ifndef DATA_QUALITY_FILTER__DATA_QUALITY_FILTER_NODE_HPP_
#define DATA_QUALITY_FILTER__DATA_QUALITY_FILTER_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include "data_quality_msgs/msg/data_quality_score.hpp"
#include "data_quality_msgs/msg/frame_quality_report.hpp"
#include "data_quality_msgs/msg/redundancy_flag.hpp"
#include "data_quality_msgs/msg/heartbeat.hpp"
#include "data_quality_msgs/srv/evaluate_data_quality.hpp"
#include "data_quality_msgs/srv/get_quality_thresholds.hpp"

namespace data_quality_filter
{

enum class State : uint8_t
{
  ACTIVE = 0,
  DEGRADED = 1,
  FAULT = 2
};

class DataQualityFilterNode : public rclcpp::Node
{
public:
  explicit DataQualityFilterNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~DataQualityFilterNode() override;

private:
  void publish_heartbeat();
  void evaluate_cycle();
  void transition_to(State new_state);

  // Service handlers
  void handle_evaluate_data_quality(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_quality_msgs::srv::EvaluateDataQuality::Request> request,
    std::shared_ptr<data_quality_msgs::srv::EvaluateDataQuality::Response> response);

  void handle_get_quality_thresholds(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_quality_msgs::srv::GetQualityThresholds::Request> request,
    std::shared_ptr<data_quality_msgs::srv::GetQualityThresholds::Response> response);

  // Topic callbacks
  void on_image(const sensor_msgs::msg::Image::SharedPtr msg);
  void on_point_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
  void on_imu(const sensor_msgs::msg::Imu::SharedPtr msg);
  void on_joint_states(const sensor_msgs::msg::JointState::SharedPtr msg);

  // Publishers
  rclcpp::Publisher<data_quality_msgs::msg::DataQualityScore>::SharedPtr score_pub_;
  rclcpp::Publisher<data_quality_msgs::msg::FrameQualityReport>::SharedPtr frame_report_pub_;
  rclcpp::Publisher<data_quality_msgs::msg::RedundancyFlag>::SharedPtr redundancy_pub_;
  rclcpp::Publisher<data_quality_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Services
  rclcpp::Service<data_quality_msgs::srv::EvaluateDataQuality>::SharedPtr evaluate_srv_;
  rclcpp::Service<data_quality_msgs::srv::GetQualityThresholds>::SharedPtr get_thresholds_srv_;

  // Subscriptions
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr point_cloud_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_states_sub_;

  // Timers
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::TimerBase::SharedPtr eval_timer_;

  // State
  std::atomic<State> state_{State::ACTIVE};
  uint32_t error_code_{0};
  uint32_t frames_evaluated_{0};
  uint32_t frames_flagged_dirty_{0};
  uint32_t frames_flagged_redundant_{0};
};

}  // namespace data_quality_filter

#endif  // DATA_QUALITY_FILTER__DATA_QUALITY_FILTER_NODE_HPP_
