#include "data_quality_filter/data_quality_filter_node.hpp"

#include <chrono>

namespace data_quality_filter
{

using namespace std::chrono_literals;

DataQualityFilterNode::DataQualityFilterNode(const rclcpp::NodeOptions &options)
: Node("data_quality_filter_node", options)
{
  // Declare parameters
  this->declare_parameter<float>("thresholds.min_sharpness", 100.0f);
  this->declare_parameter<int>("thresholds.min_density", 1000);
  this->declare_parameter<float>("thresholds.max_noise", 0.05f);
  this->declare_parameter<float>("thresholds.min_completeness", 0.9f);

  // Publishers
  score_pub_ = this->create_publisher<data_quality_msgs::msg::DataQualityScore>(
    "/data_quality/score", 10);
  frame_report_pub_ = this->create_publisher<data_quality_msgs::msg::FrameQualityReport>(
    "/data_quality/frame_report", 10);
  redundancy_pub_ = this->create_publisher<data_quality_msgs::msg::RedundancyFlag>(
    "/data_quality/redundancy_flag", 10);
  heartbeat_pub_ = this->create_publisher<data_quality_msgs::msg::Heartbeat>(
    "/data_quality/heartbeat", 10);

  // Services
  evaluate_srv_ = this->create_service<data_quality_msgs::srv::EvaluateDataQuality>(
    "/data_quality/evaluate",
    std::bind(&DataQualityFilterNode::handle_evaluate_data_quality, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_thresholds_srv_ = this->create_service<data_quality_msgs::srv::GetQualityThresholds>(
    "/data_quality/get_thresholds",
    std::bind(&DataQualityFilterNode::handle_get_quality_thresholds, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Subscriptions
  image_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
    "/perception/image_raw", rclcpp::SensorDataQoS(),
    std::bind(&DataQualityFilterNode::on_image, this, std::placeholders::_1));

  point_cloud_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
    "/perception/point_cloud", rclcpp::SensorDataQoS(),
    std::bind(&DataQualityFilterNode::on_point_cloud, this, std::placeholders::_1));

  imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
    "/hal_sensor/imu", rclcpp::SensorDataQoS(),
    std::bind(&DataQualityFilterNode::on_imu, this, std::placeholders::_1));

  joint_states_sub_ = this->create_subscription<sensor_msgs::msg::JointState>(
    "/mc/joint_states", rclcpp::SensorDataQoS(),
    std::bind(&DataQualityFilterNode::on_joint_states, this, std::placeholders::_1));

  // Timers
  heartbeat_timer_ = this->create_wall_timer(
    1s, std::bind(&DataQualityFilterNode::publish_heartbeat, this));
  eval_timer_ = this->create_wall_timer(
    1ms, std::bind(&DataQualityFilterNode::evaluate_cycle, this));

  RCLCPP_INFO(this->get_logger(), "DataQualityFilterNode initialized");
}

DataQualityFilterNode::~DataQualityFilterNode()
{
  RCLCPP_INFO(this->get_logger(), "DataQualityFilterNode shutting down");
}

void DataQualityFilterNode::publish_heartbeat()
{
  auto msg = data_quality_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.state = static_cast<uint8_t>(state_.load());
  msg.error_code = error_code_;
  msg.frames_evaluated = frames_evaluated_;
  msg.frames_flagged_dirty = frames_flagged_dirty_;
  msg.frames_flagged_redundant = frames_flagged_redundant_;
  heartbeat_pub_->publish(msg);
}

void DataQualityFilterNode::evaluate_cycle()
{
  // TODO: Implement 1kHz evaluation cycle
  frames_evaluated_++;
}

void DataQualityFilterNode::transition_to(State new_state)
{
  auto old_state = state_.exchange(new_state);
  if (old_state != new_state) {
    RCLCPP_INFO(this->get_logger(), "State transition: %d -> %d",
                static_cast<int>(old_state), static_cast<int>(new_state));
  }
}

void DataQualityFilterNode::handle_evaluate_data_quality(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_quality_msgs::srv::EvaluateDataQuality::Request> request,
  std::shared_ptr<data_quality_msgs::srv::EvaluateDataQuality::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "EvaluateDataQuality request: file=%s", request->file_path.c_str());
  // TODO: Implement file-based quality evaluation
  response->success = false;
  response->message = "Not implemented";
}

void DataQualityFilterNode::handle_get_quality_thresholds(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_quality_msgs::srv::GetQualityThresholds::Request>,
  std::shared_ptr<data_quality_msgs::srv::GetQualityThresholds::Response> response)
{
  response->success = true;
  response->min_sharpness = this->get_parameter("thresholds.min_sharpness").as_double();
  response->min_density = this->get_parameter("thresholds.min_density").as_int();
  response->max_noise = this->get_parameter("thresholds.max_noise").as_double();
  response->min_completeness = this->get_parameter("thresholds.min_completeness").as_double();
  response->redundancy_threshold = 0.95f;
}

void DataQualityFilterNode::on_image(const sensor_msgs::msg::Image::SharedPtr)
{
  // TODO: Evaluate image quality
}

void DataQualityFilterNode::on_point_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr)
{
  // TODO: Evaluate point cloud quality
}

void DataQualityFilterNode::on_imu(const sensor_msgs::msg::Imu::SharedPtr)
{
  // TODO: Evaluate IMU quality
}

void DataQualityFilterNode::on_joint_states(const sensor_msgs::msg::JointState::SharedPtr)
{
  // TODO: Check VLA frame completeness
}

}  // namespace data_quality_filter

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(data_quality_filter::DataQualityFilterNode)
