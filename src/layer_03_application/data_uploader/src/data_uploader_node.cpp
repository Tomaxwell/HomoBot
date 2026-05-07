#include "data_uploader/data_uploader_node.hpp"

#include <chrono>

namespace data_uploader
{

using namespace std::chrono_literals;

DataUploaderNode::DataUploaderNode(const rclcpp::NodeOptions & options)
: Node("data_uploader_node", options)
{
  // Declare parameters
  this->declare_parameter<std::string>("upload_mode", "adaptive");
  this->declare_parameter<int>("max_concurrent_uploads", 3);
  this->declare_parameter<int>("max_retry_count", 5);

  // Publishers
  upload_status_pub_ = this->create_publisher<data_uploader_msgs::msg::UploadStatus>(
    "/data_uploader/upload_status", 10);
  bandwidth_status_pub_ = this->create_publisher<data_uploader_msgs::msg::BandwidthStatus>(
    "/data_uploader/bandwidth_status", 10);
  heartbeat_pub_ = this->create_publisher<data_uploader_msgs::msg::Heartbeat>(
    "/data_uploader/heartbeat", 10);

  // Services
  trigger_upload_srv_ = this->create_service<data_uploader_msgs::srv::TriggerUpload>(
    "/data_uploader/trigger_upload",
    std::bind(&DataUploaderNode::handle_trigger_upload, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_upload_queue_srv_ = this->create_service<data_uploader_msgs::srv::GetUploadQueue>(
    "/data_uploader/get_upload_queue",
    std::bind(&DataUploaderNode::handle_get_upload_queue, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  cancel_upload_srv_ = this->create_service<data_uploader_msgs::srv::CancelUpload>(
    "/data_uploader/cancel_upload",
    std::bind(&DataUploaderNode::handle_cancel_upload, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Actions
  upload_dataset_action_ = rclcpp_action::create_server<data_uploader_msgs::action::UploadDataset>(
    this,
    "/data_uploader/upload_dataset",
    std::bind(&DataUploaderNode::handle_upload_dataset_goal, this,
              std::placeholders::_1, std::placeholders::_2),
    std::bind(&DataUploaderNode::handle_upload_dataset_cancel, this,
              std::placeholders::_1),
    std::bind(&DataUploaderNode::handle_upload_dataset_accepted, this,
              std::placeholders::_1));

  // Timers
  bandwidth_timer_ = this->create_wall_timer(
    1s, std::bind(&DataUploaderNode::publish_bandwidth_status, this));
  heartbeat_timer_ = this->create_wall_timer(
    1s, std::bind(&DataUploaderNode::publish_heartbeat, this));

  RCLCPP_INFO(this->get_logger(), "DataUploaderNode initialized");
}

DataUploaderNode::~DataUploaderNode()
{
  RCLCPP_INFO(this->get_logger(), "DataUploaderNode shutting down");
}

void DataUploaderNode::publish_bandwidth_status()
{
  auto msg = data_uploader_msgs::msg::BandwidthStatus();
  msg.allocated_bandwidth_bps = 0.0f;
  msg.used_bandwidth_bps = 0.0f;
  msg.available_bandwidth_bps = 0.0f;
  msg.is_limited = false;
  msg.limit_reason = "";
  bandwidth_status_pub_->publish(msg);
}

void DataUploaderNode::publish_heartbeat()
{
  auto msg = data_uploader_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.state = static_cast<uint8_t>(state_.load());
  msg.error_code = error_code_;
  msg.task_count = 0;
  heartbeat_pub_->publish(msg);
}

void DataUploaderNode::transition_to(State new_state)
{
  auto old_state = state_.exchange(new_state);
  if (old_state != new_state) {
    RCLCPP_INFO(this->get_logger(), "State transition: %d -> %d",
                static_cast<int>(old_state), static_cast<int>(new_state));
  }
}

void DataUploaderNode::handle_trigger_upload(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_uploader_msgs::srv::TriggerUpload::Request> request,
  std::shared_ptr<data_uploader_msgs::srv::TriggerUpload::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "TriggerUpload request: asset_id=%s", request->asset_id.c_str());
  // TODO: Implement upload scheduling logic
  response->success = false;
  response->task_id = "";
  response->message = "Not implemented";
}

void DataUploaderNode::handle_get_upload_queue(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_uploader_msgs::srv::GetUploadQueue::Request>,
  std::shared_ptr<data_uploader_msgs::srv::GetUploadQueue::Response> response)
{
  // TODO: Implement queue query logic
  response->success = true;
  response->total_pending = 0;
  response->total_uploading = 0;
  response->total_completed_today = 0;
  response->total_bytes_uploaded_today = 0;
}

void DataUploaderNode::handle_cancel_upload(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_uploader_msgs::srv::CancelUpload::Request> request,
  std::shared_ptr<data_uploader_msgs::srv::CancelUpload::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "CancelUpload request: task_id=%s", request->task_id.c_str());
  // TODO: Implement cancel logic
  response->success = false;
  response->message = "Not implemented";
}

rclcpp_action::GoalResponse DataUploaderNode::handle_upload_dataset_goal(
  const rclcpp_action::GoalUUID &,
  std::shared_ptr<const data_uploader_msgs::action::UploadDataset::Goal> goal)
{
  RCLCPP_INFO(this->get_logger(), "UploadDataset goal: dataset_id=%s", goal->dataset_id.c_str());
  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse DataUploaderNode::handle_upload_dataset_cancel(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<data_uploader_msgs::action::UploadDataset>>)
{
  RCLCPP_INFO(this->get_logger(), "UploadDataset cancel request");
  return rclcpp_action::CancelResponse::ACCEPT;
}

void DataUploaderNode::handle_upload_dataset_accepted(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<data_uploader_msgs::action::UploadDataset>> goal_handle)
{
  // TODO: Implement dataset upload logic with feedback
  (void)goal_handle;
  RCLCPP_INFO(this->get_logger(), "UploadDataset accepted");
}

}  // namespace data_uploader

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(data_uploader::DataUploaderNode)
