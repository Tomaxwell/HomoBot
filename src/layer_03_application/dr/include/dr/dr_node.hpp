#ifndef DR__DR_NODE_HPP_
#define DR__DR_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

#include "dr_msgs/msg/recorder_state.hpp"
#include "dr_msgs/msg/recording_session.hpp"
#include "dr_msgs/msg/vla_data_frame.hpp"
#include "dr_msgs/msg/heartbeat.hpp"
#include "dr_msgs/srv/get_health_status.hpp"
#include "dr_msgs/srv/start_recording.hpp"
#include "dr_msgs/srv/stop_recording.hpp"
#include "dr_msgs/srv/list_recordings.hpp"
#include "dr_msgs/srv/export_recording.hpp"
#include "dr_msgs/srv/delete_recording.hpp"
#include "dr_msgs/srv/tag_recording.hpp"

namespace dr
{

enum class State : uint8_t
{
  IDLE = 0,
  RECORDING = 1,
  EXPORTING = 2,
  FAULT = 3
};

class DataRecorderNode : public rclcpp::Node
{
public:
  explicit DataRecorderNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~DataRecorderNode() override;

private:
  void publish_state();
  void publish_heartbeat();
  void publish_vla_data_frame();
  void transition_to(State new_state);

  // Service handlers
  void handle_get_health_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<dr_msgs::srv::GetHealthStatus::Request> request,
    std::shared_ptr<dr_msgs::srv::GetHealthStatus::Response> response);

  void handle_start_recording(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<dr_msgs::srv::StartRecording::Request> request,
    std::shared_ptr<dr_msgs::srv::StartRecording::Response> response);

  void handle_stop_recording(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<dr_msgs::srv::StopRecording::Request> request,
    std::shared_ptr<dr_msgs::srv::StopRecording::Response> response);

  void handle_list_recordings(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<dr_msgs::srv::ListRecordings::Request> request,
    std::shared_ptr<dr_msgs::srv::ListRecordings::Response> response);

  void handle_export_recording(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<dr_msgs::srv::ExportRecording::Request> request,
    std::shared_ptr<dr_msgs::srv::ExportRecording::Response> response);

  void handle_delete_recording(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<dr_msgs::srv::DeleteRecording::Request> request,
    std::shared_ptr<dr_msgs::srv::DeleteRecording::Response> response);

  void handle_tag_recording(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<dr_msgs::srv::TagRecording::Request> request,
    std::shared_ptr<dr_msgs::srv::TagRecording::Response> response);

  // Topic subscribers (for recording)
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr point_cloud_sub_;

  void on_image(const sensor_msgs::msg::Image::SharedPtr msg);
  void on_point_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg);

  // Publishers
  rclcpp::Publisher<dr_msgs::msg::RecorderState>::SharedPtr state_pub_;
  rclcpp::Publisher<dr_msgs::msg::VlaDataFrame>::SharedPtr vla_data_pub_;
  rclcpp::Publisher<dr_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Service servers
  rclcpp::Service<dr_msgs::srv::GetHealthStatus>::SharedPtr get_health_status_srv_;
  rclcpp::Service<dr_msgs::srv::StartRecording>::SharedPtr start_recording_srv_;
  rclcpp::Service<dr_msgs::srv::StopRecording>::SharedPtr stop_recording_srv_;
  rclcpp::Service<dr_msgs::srv::ListRecordings>::SharedPtr list_recordings_srv_;
  rclcpp::Service<dr_msgs::srv::ExportRecording>::SharedPtr export_recording_srv_;
  rclcpp::Service<dr_msgs::srv::DeleteRecording>::SharedPtr delete_recording_srv_;
  rclcpp::Service<dr_msgs::srv::TagRecording>::SharedPtr tag_recording_srv_;

  // Timers
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::TimerBase::SharedPtr vla_timer_;

  // State
  State current_state_{State::IDLE};
  State prev_state_{State::IDLE};
  builtin_interfaces::msg::Time state_changed_at_;
  rclcpp::Time uptime_start_;

  // Recording session
  dr_msgs::msg::RecordingSession current_session_;
  bool is_recording_{false};
  std::vector<dr_msgs::msg::RecordingSession> sessions_;
  std::mutex session_mutex_;

  // Parameters
  double heartbeat_rate_hz_{1.0};
  double vla_publish_rate_hz_{10.0};
  std::string recording_base_path_{"/opt/striding/recordings"};
  float max_storage_usage_percent_{90.0};
};

}  // namespace dr

#endif  // DR__DR_NODE_HPP_
