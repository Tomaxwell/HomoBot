#ifndef DATA_UPLOADER__DATA_UPLOADER_NODE_HPP_
#define DATA_UPLOADER__DATA_UPLOADER_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include "data_uploader_msgs/msg/upload_status.hpp"
#include "data_uploader_msgs/msg/data_asset_info.hpp"
#include "data_uploader_msgs/msg/bandwidth_status.hpp"
#include "data_uploader_msgs/msg/heartbeat.hpp"
#include "data_uploader_msgs/srv/trigger_upload.hpp"
#include "data_uploader_msgs/srv/get_upload_queue.hpp"
#include "data_uploader_msgs/srv/cancel_upload.hpp"
#include "data_uploader_msgs/action/upload_dataset.hpp"

namespace data_uploader
{

enum class State : uint8_t
{
  STANDBY = 0,
  ACTIVE = 1,
  BANDWIDTH_LIMITED = 2,
  OFFLINE = 3,
  FAULT = 4
};

class DataUploaderNode : public rclcpp::Node
{
public:
  explicit DataUploaderNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~DataUploaderNode() override;

private:
  void publish_bandwidth_status();
  void publish_heartbeat();
  void transition_to(State new_state);

  // Service handlers
  void handle_trigger_upload(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_uploader_msgs::srv::TriggerUpload::Request> request,
    std::shared_ptr<data_uploader_msgs::srv::TriggerUpload::Response> response);

  void handle_get_upload_queue(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_uploader_msgs::srv::GetUploadQueue::Request> request,
    std::shared_ptr<data_uploader_msgs::srv::GetUploadQueue::Response> response);

  void handle_cancel_upload(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_uploader_msgs::srv::CancelUpload::Request> request,
    std::shared_ptr<data_uploader_msgs::srv::CancelUpload::Response> response);

  // Action handlers
  rclcpp_action::GoalResponse handle_upload_dataset_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const data_uploader_msgs::action::UploadDataset::Goal> goal);

  rclcpp_action::CancelResponse handle_upload_dataset_cancel(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<data_uploader_msgs::action::UploadDataset>> goal_handle);

  void handle_upload_dataset_accepted(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<data_uploader_msgs::action::UploadDataset>> goal_handle);

  // Internal components (TODO: implement)
  // std::unique_ptr<UploadManager> upload_manager_;
  // std::unique_ptr<CompressionEngine> compression_engine_;
  // std::unique_ptr<ChunkUploader> chunk_uploader_;

  // Publishers
  rclcpp::Publisher<data_uploader_msgs::msg::UploadStatus>::SharedPtr upload_status_pub_;
  rclcpp::Publisher<data_uploader_msgs::msg::BandwidthStatus>::SharedPtr bandwidth_status_pub_;
  rclcpp::Publisher<data_uploader_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Services
  rclcpp::Service<data_uploader_msgs::srv::TriggerUpload>::SharedPtr trigger_upload_srv_;
  rclcpp::Service<data_uploader_msgs::srv::GetUploadQueue>::SharedPtr get_upload_queue_srv_;
  rclcpp::Service<data_uploader_msgs::srv::CancelUpload>::SharedPtr cancel_upload_srv_;

  // Actions
  rclcpp_action::Server<data_uploader_msgs::action::UploadDataset>::SharedPtr upload_dataset_action_;

  // Timers
  rclcpp::TimerBase::SharedPtr bandwidth_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // State
  std::atomic<State> state_{State::STANDBY};
  uint32_t error_code_{0};
};

}  // namespace data_uploader

#endif  // DATA_UPLOADER__DATA_UPLOADER_NODE_HPP_
