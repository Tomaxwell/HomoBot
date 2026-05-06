#include "ms/ms_node.hpp"

#include <chrono>
#include <algorithm>

namespace ms
{

MotionStreamerNode::MotionStreamerNode(const rclcpp::NodeOptions &options)
: Node("ms_node", options)
{
  this->declare_parameter("output_frequency_hz", 100.0);
  this->declare_parameter("max_velocity", 10.0);
  this->declare_parameter("max_acceleration", 50.0);
  this->declare_parameter("max_jerk", 200.0);
  this->declare_parameter("max_buffer_depth", 100);
  this->declare_parameter("heartbeat_rate_hz", 1.0);
  this->declare_parameter("statistics_rate_hz", 1.0);

  output_frequency_hz_ = this->get_parameter("output_frequency_hz").as_double();
  max_velocity_ = this->get_parameter("max_velocity").as_double();
  max_acceleration_ = this->get_parameter("max_acceleration").as_double();
  max_jerk_ = this->get_parameter("max_jerk").as_double();
  max_buffer_depth_ = this->get_parameter("max_buffer_depth").as_int();
  heartbeat_rate_hz_ = this->get_parameter("heartbeat_rate_hz").as_double();
  statistics_rate_hz_ = this->get_parameter("statistics_rate_hz").as_double();

  uptime_start_ = this->now();
  state_changed_at_ = this->now();

  // Subscribers
  motion_command_sub_ = this->create_subscription<ms_msgs::msg::MotionCommand>(
    "/ms/motion_command", rclcpp::QoS(10),
    std::bind(&MotionStreamerNode::on_motion_command, this, std::placeholders::_1));

  // Publishers
  motion_target_pub_ = this->create_publisher<ms_msgs::msg::MotionTarget>(
    "/ms/motion_target", rclcpp::QoS(1));

  state_pub_ = this->create_publisher<ms_msgs::msg::MotionStreamerState>(
    "/ms/motion_streamer_state", rclcpp::QoS(1).transient_local().reliable());

  statistics_pub_ = this->create_publisher<ms_msgs::msg::StreamStatistics>(
    "/ms/stream_statistics", rclcpp::QoS(1));

  heartbeat_pub_ = this->create_publisher<ms_msgs::msg::Heartbeat>(
    "/ms/heartbeat", rclcpp::QoS(1));

  // Service servers
  get_health_status_srv_ = this->create_service<ms_msgs::srv::GetHealthStatus>(
    "/ms/get_health_status",
    std::bind(&MotionStreamerNode::handle_get_health_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  set_stream_parameters_srv_ = this->create_service<ms_msgs::srv::SetStreamParameters>(
    "/ms/set_stream_parameters",
    std::bind(&MotionStreamerNode::handle_set_stream_parameters, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  stop_stream_srv_ = this->create_service<ms_msgs::srv::StopStream>(
    "/ms/stop_stream",
    std::bind(&MotionStreamerNode::handle_stop_stream, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Timers
  target_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / output_frequency_hz_),
    std::bind(&MotionStreamerNode::publish_target, this));

  statistics_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / statistics_rate_hz_),
    std::bind(&MotionStreamerNode::publish_statistics, this));

  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / heartbeat_rate_hz_),
    std::bind(&MotionStreamerNode::publish_heartbeat, this));

  RCLCPP_INFO(this->get_logger(), "MotionStreamer node initialized");
}

MotionStreamerNode::~MotionStreamerNode()
{
  RCLCPP_INFO(this->get_logger(), "MotionStreamer node shutting down");
}

void MotionStreamerNode::on_motion_command(const ms_msgs::msg::MotionCommand::SharedPtr msg)
{
  std::lock_guard<std::mutex> lock(buffer_mutex_);

  auto &buffer = source_buffers_[msg->source_id];
  if (buffer.size() >= max_buffer_depth_) {
    buffer.pop();
    commands_dropped_++;
  }

  CommandEntry entry;
  entry.command = *msg;
  entry.received_time = this->now();
  buffer.push(entry);
  commands_received_++;
}

void MotionStreamerNode::publish_target()
{
  std::lock_guard<std::mutex> lock(buffer_mutex_);

  // Find highest priority command
  CommandEntry *best_entry = nullptr;
  for (auto & [source_id, buffer] : source_buffers_) {
    if (!buffer.empty()) {
      if (!best_entry || buffer.front().command.priority > best_entry->command.priority) {
        best_entry = &buffer.front();
      }
    }
  }

  if (!best_entry) {
    return;
  }

  auto target = ms_msgs::msg::MotionTarget();
  target.stamp = this->now();
  target.positions = best_entry->command.target_positions;
  target.velocities = best_entry->command.target_velocities;
  target.torques = best_entry->command.target_torques;
  target.interpolation_type = ms_msgs::msg::MotionTarget::INTERP_MIN_JERK;

  // TODO: Apply velocity/acceleration/jerk limits

  motion_target_pub_->publish(target);

  // Remove processed command from all buffers (FIFO per source)
  for (auto & [source_id, buffer] : source_buffers_) {
    if (!buffer.empty()) {
      buffer.pop();
    }
  }
}

void MotionStreamerNode::publish_state()
{
  auto msg = ms_msgs::msg::MotionStreamerState();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.prev_state = static_cast<uint8_t>(prev_state_);
  msg.state_changed_at = state_changed_at_;
  msg.output_frequency = output_frequency_hz_;

  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t total = 0;
    for (const auto & [id, buf] : source_buffers_) {
      total += buf.size();
    }
    msg.buffer_usage = static_cast<float>(total) / static_cast<float>(max_buffer_depth_);
    msg.active_sources = source_buffers_.size();
  }

  state_pub_->publish(msg);
}

void MotionStreamerNode::publish_statistics()
{
  auto msg = ms_msgs::msg::StreamStatistics();
  msg.stamp = this->now();
  msg.output_frequency = output_frequency_hz_;

  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t total = 0;
    for (const auto & [id, buf] : source_buffers_) {
      total += buf.size();
    }
    msg.buffer_depth = total;
  }

  msg.drop_rate = (commands_received_ > 0)
    ? static_cast<float>(commands_dropped_) / commands_received_ : 0.0f;

  statistics_pub_->publish(msg);
}

void MotionStreamerNode::publish_heartbeat()
{
  auto msg = ms_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.node_name = this->get_name();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.output_frequency = output_frequency_hz_;
  heartbeat_pub_->publish(msg);
}

void MotionStreamerNode::transition_to(State new_state)
{
  if (current_state_ != new_state) {
    prev_state_ = current_state_;
    current_state_ = new_state;
    state_changed_at_ = this->now();
    publish_state();
  }
}

bool MotionStreamerNode::is_motion_allowed()
{
  return current_state_ != State::FAULT;
}

void MotionStreamerNode::handle_get_health_status(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::GetHealthStatus::Request>,
  std::shared_ptr<ms_msgs::srv::GetHealthStatus::Response> response)
{
  response->healthy = current_state_ != State::FAULT;
  response->node_name = this->get_name();
  response->state = static_cast<uint8_t>(current_state_);
  response->avg_output_frequency = output_frequency_hz_;
  response->avg_latency_ms = 0.0f;  // TODO: Calculate actual latency
}

void MotionStreamerNode::handle_set_stream_parameters(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::SetStreamParameters::Request> request,
  std::shared_ptr<ms_msgs::srv::SetStreamParameters::Response> response)
{
  if (request->output_frequency > 0) {
    output_frequency_hz_ = request->output_frequency;
    target_timer_->cancel();
    target_timer_ = this->create_wall_timer(
      std::chrono::duration<double>(1.0 / output_frequency_hz_),
      std::bind(&MotionStreamerNode::publish_target, this));
  }
  if (request->max_velocity > 0) {
    max_velocity_ = request->max_velocity;
  }
  if (request->max_acceleration > 0) {
    max_acceleration_ = request->max_acceleration;
  }
  if (request->max_jerk > 0) {
    max_jerk_ = request->max_jerk;
  }

  response->success = true;
  response->error_code = 0;
  response->message = "Parameters updated";
}

void MotionStreamerNode::handle_stop_stream(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::StopStream::Request> request,
  std::shared_ptr<ms_msgs::srv::StopStream::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "Stop stream requested (emergency=%d)", request->emergency);

  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    for (auto & [id, buffer] : source_buffers_) {
      while (!buffer.empty()) {
        buffer.pop();
      }
    }
  }

  transition_to(State::IDLE);
  response->stopped = true;
  response->error_code = 0;
  response->message = "Stream stopped";
}

}  // namespace ms

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(ms::MotionStreamerNode)
