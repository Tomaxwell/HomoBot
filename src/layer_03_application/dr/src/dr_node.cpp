#include "dr/dr_node.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace dr
{

DataRecorderNode::DataRecorderNode(const rclcpp::NodeOptions &options)
: Node("dr_node", options)
{
  this->declare_parameter("heartbeat_rate_hz", 1.0);
  this->declare_parameter("vla_publish_rate_hz", 10.0);
  this->declare_parameter("recording_base_path", "/opt/striding/recordings");
  this->declare_parameter("max_storage_usage_percent", 90.0);

  heartbeat_rate_hz_ = this->get_parameter("heartbeat_rate_hz").as_double();
  vla_publish_rate_hz_ = this->get_parameter("vla_publish_rate_hz").as_double();
  recording_base_path_ = this->get_parameter("recording_base_path").as_string();
  max_storage_usage_percent_ = this->get_parameter("max_storage_usage_percent").as_double();

  uptime_start_ = this->now();
  state_changed_at_ = this->now();

  // Subscribers
  image_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
    "/camera/image_raw", rclcpp::QoS(10),
    std::bind(&DataRecorderNode::on_image, this, std::placeholders::_1));

  point_cloud_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
    "/lidar/point_cloud", rclcpp::QoS(10),
    std::bind(&DataRecorderNode::on_point_cloud, this, std::placeholders::_1));

  // Publishers
  state_pub_ = this->create_publisher<dr_msgs::msg::RecorderState>(
    "/dr/recorder_state", rclcpp::QoS(1).transient_local().reliable());

  vla_data_pub_ = this->create_publisher<dr_msgs::msg::VlaDataFrame>(
    "/dr/vla_data_frame", rclcpp::QoS(10));

  heartbeat_pub_ = this->create_publisher<dr_msgs::msg::Heartbeat>(
    "/dr/heartbeat", rclcpp::QoS(1));

  // Service servers
  get_health_status_srv_ = this->create_service<dr_msgs::srv::GetHealthStatus>(
    "/dr/get_health_status",
    std::bind(&DataRecorderNode::handle_get_health_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  start_recording_srv_ = this->create_service<dr_msgs::srv::StartRecording>(
    "/dr/start_recording",
    std::bind(&DataRecorderNode::handle_start_recording, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  stop_recording_srv_ = this->create_service<dr_msgs::srv::StopRecording>(
    "/dr/stop_recording",
    std::bind(&DataRecorderNode::handle_stop_recording, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  list_recordings_srv_ = this->create_service<dr_msgs::srv::ListRecordings>(
    "/dr/list_recordings",
    std::bind(&DataRecorderNode::handle_list_recordings, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  export_recording_srv_ = this->create_service<dr_msgs::srv::ExportRecording>(
    "/dr/export_recording",
    std::bind(&DataRecorderNode::handle_export_recording, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  delete_recording_srv_ = this->create_service<dr_msgs::srv::DeleteRecording>(
    "/dr/delete_recording",
    std::bind(&DataRecorderNode::handle_delete_recording, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  tag_recording_srv_ = this->create_service<dr_msgs::srv::TagRecording>(
    "/dr/tag_recording",
    std::bind(&DataRecorderNode::handle_tag_recording, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Timers
  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / heartbeat_rate_hz_),
    std::bind(&DataRecorderNode::publish_heartbeat, this));

  vla_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / vla_publish_rate_hz_),
    std::bind(&DataRecorderNode::publish_vla_data_frame, this));

  RCLCPP_INFO(this->get_logger(), "DataRecorder node initialized");
}

DataRecorderNode::~DataRecorderNode()
{
  if (is_recording_) {
    RCLCPP_WARN(this->get_logger(), "Shutting down while recording - stopping session");
  }
  RCLCPP_INFO(this->get_logger(), "DataRecorder node shutting down");
}

void DataRecorderNode::publish_state()
{
  auto msg = dr_msgs::msg::RecorderState();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.prev_state = static_cast<uint8_t>(prev_state_);
  msg.state_changed_at = state_changed_at_;

  if (is_recording_) {
    msg.session_id = current_session_.session_id;
    auto now = this->now();
    auto start = rclcpp::Time(current_session_.started_at);
    msg.record_duration_sec = static_cast<float>((now - start).seconds());
  }

  state_pub_->publish(msg);
}

void DataRecorderNode::publish_heartbeat()
{
  auto msg = dr_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.node_name = this->get_name();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.storage_usage_percent = 0.0f;  // TODO: Calculate actual storage usage
  heartbeat_pub_->publish(msg);

  if (is_recording_) {
    publish_state();
  }
}

void DataRecorderNode::publish_vla_data_frame()
{
  if (!is_recording_ || !current_session_.is_vla_data) {
    return;
  }

  auto msg = dr_msgs::msg::VlaDataFrame();
  msg.stamp = this->now();
  msg.language_instruction = "placeholder";
  msg.task_phase = "middle";
  msg.is_keyframe = false;

  vla_data_pub_->publish(msg);
}

void DataRecorderNode::transition_to(State new_state)
{
  if (current_state_ != new_state) {
    prev_state_ = current_state_;
    current_state_ = new_state;
    state_changed_at_ = this->now();
    publish_state();
  }
}

void DataRecorderNode::on_image(const sensor_msgs::msg::Image::SharedPtr /*msg*/)
{
  if (!is_recording_) return;
  // TODO: Store image for recording
}

void DataRecorderNode::on_point_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr /*msg*/)
{
  if (!is_recording_) return;
  // TODO: Store point cloud for recording
}

void DataRecorderNode::handle_get_health_status(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<dr_msgs::srv::GetHealthStatus::Request>,
  std::shared_ptr<dr_msgs::srv::GetHealthStatus::Response> response)
{
  response->healthy = current_state_ != State::FAULT;
  response->node_name = this->get_name();
  response->state = static_cast<uint8_t>(current_state_);
  response->storage_usage_percent = 0.0f;
}

void DataRecorderNode::handle_start_recording(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<dr_msgs::srv::StartRecording::Request> request,
  std::shared_ptr<dr_msgs::srv::StartRecording::Response> response)
{
  if (is_recording_) {
    response->success = false;
    response->error_code = 13001;  // ALREADY_RECORDING
    response->message = "Already recording";
    return;
  }

  std::lock_guard<std::mutex> lock(session_mutex_);

  auto session = dr_msgs::msg::RecordingSession();
  session.session_id = "rec_" + std::to_string(this->now().nanoseconds());
  session.name = request->name;
  session.description = request->description;
  session.started_at = this->now();
  session.trigger_type = request->trigger_type;
  session.task_id = request->task_id;
  session.tags = request->tags;
  session.is_vla_data = request->is_vla_mode;
  session.topics_recorded = request->topics;

  current_session_ = session;
  is_recording_ = true;

  transition_to(State::RECORDING);

  response->success = true;
  response->error_code = 0;
  response->message = "Recording started";
  response->session_id = session.session_id;

  RCLCPP_INFO(this->get_logger(), "Started recording session: %s", session.session_id.c_str());
}

void DataRecorderNode::handle_stop_recording(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<dr_msgs::srv::StopRecording::Request> request,
  std::shared_ptr<dr_msgs::srv::StopRecording::Response> response)
{
  if (!is_recording_ || current_session_.session_id != request->session_id) {
    response->success = false;
    response->error_code = 13002;  // NOT_RECORDING
    response->message = "Not recording or session ID mismatch";
    return;
  }

  std::lock_guard<std::mutex> lock(session_mutex_);

  current_session_.stopped_at = this->now();
  auto start = rclcpp::Time(current_session_.started_at);
  auto stop = rclcpp::Time(current_session_.stopped_at);
  current_session_.duration_sec = static_cast<float>((stop - start).seconds());

  sessions_.push_back(current_session_);

  response->success = true;
  response->error_code = 0;
  response->message = "Recording stopped";
  response->session_info = current_session_;

  is_recording_ = false;
  transition_to(State::IDLE);

  RCLCPP_INFO(this->get_logger(), "Stopped recording session: %s", request->session_id.c_str());
}

void DataRecorderNode::handle_list_recordings(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<dr_msgs::srv::ListRecordings::Request>,
  std::shared_ptr<dr_msgs::srv::ListRecordings::Response> response)
{
  std::lock_guard<std::mutex> lock(session_mutex_);
  response->success = true;
  response->sessions = sessions_;
  response->count = sessions_.size();
}

void DataRecorderNode::handle_export_recording(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<dr_msgs::srv::ExportRecording::Request> request,
  std::shared_ptr<dr_msgs::srv::ExportRecording::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "Exporting session %s to %s in format %s",
              request->session_id.c_str(), request->export_path.c_str(), request->format.c_str());

  // TODO: Implement actual export logic
  response->success = true;
  response->error_code = 0;
  response->message = "Export initiated";
  response->bytes_exported = 0;
}

void DataRecorderNode::handle_delete_recording(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<dr_msgs::srv::DeleteRecording::Request> request,
  std::shared_ptr<dr_msgs::srv::DeleteRecording::Response> response)
{
  if (!request->confirm) {
    response->success = false;
    response->error_code = 13003;  // CONFIRMATION_REQUIRED
    response->message = "Deletion requires confirmation";
    return;
  }

  std::lock_guard<std::mutex> lock(session_mutex_);
  auto it = std::remove_if(sessions_.begin(), sessions_.end(),
    [&request](const auto &s) { return s.session_id == request->session_id; });

  if (it == sessions_.end()) {
    response->success = false;
    response->error_code = 13004;  // SESSION_NOT_FOUND
    response->message = "Session not found";
    return;
  }

  sessions_.erase(it, sessions_.end());
  response->success = true;
  response->error_code = 0;
  response->message = "Session deleted";
}

void DataRecorderNode::handle_tag_recording(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<dr_msgs::srv::TagRecording::Request> request,
  std::shared_ptr<dr_msgs::srv::TagRecording::Response> response)
{
  std::lock_guard<std::mutex> lock(session_mutex_);
  for (auto &session : sessions_) {
    if (session.session_id == request->session_id) {
      for (const auto &tag : request->tags_to_add) {
        session.tags.push_back(tag);
      }
      response->success = true;
      response->error_code = 0;
      response->message = "Tags added";
      return;
    }
  }

  response->success = false;
  response->error_code = 13004;  // SESSION_NOT_FOUND
  response->message = "Session not found";
}

}  // namespace dr

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(dr::DataRecorderNode)
