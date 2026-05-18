#include "ms/ms_node.hpp"

#include <chrono>
#include <random>
#include <thread>

namespace ms
{

// ── Constructor ──────────────────────────────────────────────

MotionStreamerNode::MotionStreamerNode(const rclcpp::NodeOptions &options)
: Node("ms_node", options)
{
  // Parameters
  this->declare_parameter("device_poll_rate_hz", 60.0);
  this->declare_parameter("teleop_state_rate_hz", 10.0);
  this->declare_parameter("device_state_rate_hz", 10.0);
  this->declare_parameter("heartbeat_rate_hz", 1.0);
  this->declare_parameter("tracking_quality_threshold", 0.5f);
  this->declare_parameter("max_dropped_frames", 30);

  device_poll_rate_hz_ = this->get_parameter("device_poll_rate_hz").as_double();
  teleop_state_rate_hz_ = this->get_parameter("teleop_state_rate_hz").as_double();
  device_state_rate_hz_ = this->get_parameter("device_state_rate_hz").as_double();
  heartbeat_rate_hz_ = this->get_parameter("heartbeat_rate_hz").as_double();
  tracking_quality_threshold_ = this->get_parameter("tracking_quality_threshold").as_float();
  max_dropped_frames_ = static_cast<uint32_t>(this->get_parameter("max_dropped_frames").as_int());

  uptime_start_ = this->now();
  state_changed_at_ = this->now();

  // Publishers
  motion_target_pub_ = this->create_publisher<mc_msgs::msg::MotionTarget>(
    "/ms/motion_target", rclcpp::QoS(1).best_effort());

  teleop_state_pub_ = this->create_publisher<ms_msgs::msg::TeleopState>(
    "/ms/teleop_state", rclcpp::QoS(1).transient_local().reliable());

  device_state_pub_ = this->create_publisher<ms_msgs::msg::DeviceState>(
    "/ms/device_state", rclcpp::QoS(1));

  heartbeat_pub_ = this->create_publisher<ms_msgs::msg::Heartbeat>(
    "/ms/heartbeat", rclcpp::QoS(1));

  // Service servers
  start_teleop_srv_ = this->create_service<ms_msgs::srv::StartTeleop>(
    "/ms/start_teleop",
    std::bind(&MotionStreamerNode::handle_start_teleop, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  stop_teleop_srv_ = this->create_service<ms_msgs::srv::StopTeleop>(
    "/ms/stop_teleop",
    std::bind(&MotionStreamerNode::handle_stop_teleop, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  pause_teleop_srv_ = this->create_service<ms_msgs::srv::PauseTeleop>(
    "/ms/pause_teleop",
    std::bind(&MotionStreamerNode::handle_pause_teleop, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_device_status_srv_ = this->create_service<ms_msgs::srv::GetDeviceStatus>(
    "/ms/get_device_status",
    std::bind(&MotionStreamerNode::handle_get_device_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_health_status_srv_ = this->create_service<ms_msgs::srv::GetHealthStatus>(
    "/ms/get_health_status",
    std::bind(&MotionStreamerNode::handle_get_health_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Action server
  execute_teleop_srv_ = rclcpp_action::create_server<ms_msgs::action::ExecuteTeleop>(
    this,
    "/ms/execute_teleop",
    std::bind(&MotionStreamerNode::handle_execute_goal, this,
              std::placeholders::_1, std::placeholders::_2),
    std::bind(&MotionStreamerNode::handle_execute_cancel, this,
              std::placeholders::_1),
    std::bind(&MotionStreamerNode::handle_execute_accepted, this,
              std::placeholders::_1));

  // Timers
  device_poll_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / device_poll_rate_hz_),
    std::bind(&MotionStreamerNode::on_device_poll, this));
  device_poll_timer_->cancel();  // Only active during STREAMING

  teleop_state_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / teleop_state_rate_hz_),
    std::bind(&MotionStreamerNode::on_publish_teleop_state, this));

  device_state_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / device_state_rate_hz_),
    std::bind(&MotionStreamerNode::on_publish_device_state, this));

  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / heartbeat_rate_hz_),
    std::bind(&MotionStreamerNode::on_publish_heartbeat, this));

  RCLCPP_INFO(this->get_logger(), "MotionStreamer node initialized (VR/mocap teleop)");
}

// ── Destructor ───────────────────────────────────────────────

MotionStreamerNode::~MotionStreamerNode()
{
  disconnect_device();
  RCLCPP_INFO(this->get_logger(), "MotionStreamer node shutting down");
}

// ── State Machine ────────────────────────────────────────────

void MotionStreamerNode::transition_to(State new_state)
{
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (current_state_ == new_state) {
    return;
  }

  prev_state_ = current_state_;
  current_state_ = new_state;
  state_changed_at_ = this->now();

  RCLCPP_INFO(this->get_logger(), "State: %u → %u",
              static_cast<uint8_t>(prev_state_), static_cast<uint8_t>(current_state_));

  // Timer control
  if (current_state_ == State::STREAMING) {
    device_poll_timer_->reset();
  } else {
    device_poll_timer_->cancel();
  }

  on_publish_teleop_state();
}

bool MotionStreamerNode::is_motion_allowed() const
{
  return current_state_ == State::STREAMING;
}

// ── Device I/O Stubs ─────────────────────────────────────────

bool MotionStreamerNode::connect_device(
  const std::string &device_id,
  const std::string &device_type)
{
  // TODO: Integrate actual VR/mocap SDK (OpenVR, Quest Link, Xsens, Noitom, etc.)
  RCLCPP_INFO(this->get_logger(), "Connecting device '%s' (type: %s)",
              device_id.c_str(), device_type.c_str());
  device_id_ = device_id;
  device_type_ = device_type;
  tracking_quality_ = 0.0f;
  frame_count_ = 0;
  return true;  // Stub: always succeeds
}

void MotionStreamerNode::disconnect_device()
{
  // TODO: Teardown actual device connection
  RCLCPP_INFO(this->get_logger(), "Disconnecting device '%s'", device_id_.c_str());
  device_id_.clear();
  device_type_.clear();
  tracking_quality_ = 0.0f;
}

bool MotionStreamerNode::poll_device_data()
{
  // TODO: Poll actual VR/mocap device for new frame
  // Stub: simulate occasional tracking quality changes
  static std::mt19937 rng(static_cast<uint32_t>(this->now().nanoseconds()));
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  tracking_quality_ = dist(rng);
  frame_count_++;
  return tracking_quality_ >= tracking_quality_threshold_;
}

float MotionStreamerNode::get_device_tracking_quality() const
{
  return tracking_quality_;
}

// ── Retargeting Stub ─────────────────────────────────────────

mc_msgs::msg::MotionTarget MotionStreamerNode::retarget_human_pose()
{
  // TODO: Replace with actual human→robot motion retargeting engine
  // - Parse human skeleton from device data
  // - Map to robot URDF joint space
  // - Handle limb length differences via scale factors
  // - Apply mirror_mode if configured
  mc_msgs::msg::MotionTarget target;
  target.stamp = this->now();
  target.source_id = "ms";
  target.control_type = 0;  // 位置优先
  // Stub: empty positions — real implementation fills from retargeting
  return target;
}

// ── Timer Callbacks ──────────────────────────────────────────

void MotionStreamerNode::on_device_poll()
{
  if (current_state_ != State::STREAMING) {
    return;
  }

  bool valid_frame = poll_device_data();
  if (!valid_frame) {
    if (tracking_quality_ < 0.1f) {
      RCLCPP_WARN(this->get_logger(), "Tracking lost — entering ERROR state");
      transition_to(State::ERROR);
    }
    return;
  }

  auto target = retarget_human_pose();
  motion_target_pub_->publish(target);
}

void MotionStreamerNode::on_publish_teleop_state()
{
  // Note: not locking state_mutex_ here — called from transition_to (which holds lock)
  // and from timer callbacks that run on the same executor thread.
  auto msg = ms_msgs::msg::TeleopState();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.prev_state = static_cast<uint8_t>(prev_state_);
  msg.state_changed_at = state_changed_at_;
  msg.device_id = device_id_;
  msg.device_type = device_type_;
  msg.device_connected = !device_id_.empty();
  msg.tracking_quality = tracking_quality_;
  msg.frame_count = frame_count_;
  msg.stream_frequency = static_cast<float>(device_poll_rate_hz_);

  teleop_state_pub_->publish(msg);
}

void MotionStreamerNode::on_publish_device_state()
{
  auto msg = ms_msgs::msg::DeviceState();
  msg.stamp = this->now();
  msg.device_id = device_id_;
  msg.device_type = device_type_;
  msg.connected = !device_id_.empty();
  msg.battery_percent = 0.0f;     // TODO: from actual device
  msg.tracking_state = (current_state_ == State::STREAMING) ? 2 : 0;
  msg.tracking_quality = tracking_quality_;
  msg.dropped_frames = 0;         // TODO: from actual device
  msg.latency_ms = 0.0f;          // TODO: measure actual latency

  device_state_pub_->publish(msg);
}

void MotionStreamerNode::on_publish_heartbeat()
{
  auto msg = ms_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.node_name = this->get_name();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.healthy = (current_state_ != State::ERROR);
  msg.status_message = device_id_.empty() ? "idle" : ("device: " + device_id_);
  msg.output_frequency = static_cast<float>(device_poll_rate_hz_);

  heartbeat_pub_->publish(msg);
}

// ── Service Handlers ─────────────────────────────────────────

void MotionStreamerNode::handle_start_teleop(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::StartTeleop::Request> request,
  std::shared_ptr<ms_msgs::srv::StartTeleop::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "StartTeleop: device=%s type=%s",
              request->device_id.c_str(), request->device_type.c_str());

  if (current_state_ != State::IDLE && current_state_ != State::ERROR) {
    response->success = false;
    response->error_code = 1;  // Already in session
    response->message = "Already in a teleop session";
    return;
  }

  transition_to(State::CONNECTING);

  if (!connect_device(request->device_id, request->device_type)) {
    transition_to(State::ERROR);
    response->success = false;
    response->error_code = 2;  // Connection failed
    response->message = "Failed to connect device";
    return;
  }

  retargeting_config_ = request->config;
  session_id_ = request->device_id + "_" + std::to_string(this->now().nanoseconds());

  transition_to(State::READY);
  response->success = true;
  response->error_code = 0;
  response->message = "Device connected, ready to stream";
  response->session_id = session_id_;
}

void MotionStreamerNode::handle_stop_teleop(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::StopTeleop::Request> request,
  std::shared_ptr<ms_msgs::srv::StopTeleop::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "StopTeleop: session=%s emergency=%d",
              request->session_id.c_str(), request->emergency);

  if (current_state_ == State::IDLE) {
    response->stopped = true;
    response->error_code = 0;
    response->message = "Already idle";
    return;
  }

  disconnect_device();
  session_id_.clear();
  frame_count_ = 0;
  transition_to(State::IDLE);

  response->stopped = true;
  response->error_code = 0;
  response->message = request->emergency ? "Emergency stopped" : "Session stopped";
}

void MotionStreamerNode::handle_pause_teleop(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::PauseTeleop::Request> request,
  std::shared_ptr<ms_msgs::srv::PauseTeleop::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "PauseTeleop: session=%s pause=%d",
              request->session_id.c_str(), request->pause);

  if (request->pause) {
    if (current_state_ == State::STREAMING) {
      transition_to(State::PAUSED);
      response->success = true;
      response->state = static_cast<uint8_t>(State::PAUSED);
    } else {
      response->success = false;
      response->error_code = 3;  // Invalid state transition
      response->message = "Cannot pause — not streaming";
      response->state = static_cast<uint8_t>(current_state_);
    }
  } else {
    if (current_state_ == State::PAUSED) {
      transition_to(State::STREAMING);
      response->success = true;
      response->state = static_cast<uint8_t>(State::STREAMING);
    } else {
      response->success = false;
      response->error_code = 3;
      response->message = "Cannot resume — not paused";
      response->state = static_cast<uint8_t>(current_state_);
    }
  }
}

void MotionStreamerNode::handle_get_device_status(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::GetDeviceStatus::Request> request,
  std::shared_ptr<ms_msgs::srv::GetDeviceStatus::Response> response)
{
  auto device = ms_msgs::msg::DeviceState();
  device.stamp = this->now();
  device.device_id = device_id_;
  device.device_type = device_type_;
  device.connected = !device_id_.empty();
  device.tracking_quality = tracking_quality_;

  if (request->device_id.empty() || request->device_id == device_id_) {
    response->success = true;
    response->devices.push_back(device);
    response->count = 1;
  } else {
    response->success = false;
    response->count = 0;
  }
}

void MotionStreamerNode::handle_get_health_status(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<ms_msgs::srv::GetHealthStatus::Request>,
  std::shared_ptr<ms_msgs::srv::GetHealthStatus::Response> response)
{
  response->success = true;
  response->node_name = this->get_name();
  response->uptime_since.sec = static_cast<int32_t>(uptime_start_.seconds());
  response->uptime_since.nanosec = static_cast<uint32_t>(uptime_start_.nanoseconds() % 1000000000ULL);
  response->state = static_cast<uint8_t>(current_state_);
  response->healthy = (current_state_ != State::ERROR);
  response->message = device_id_.empty() ? "idle" : "active";
  response->avg_output_frequency = static_cast<float>(device_poll_rate_hz_);
  response->avg_latency_ms = 0.0f;  // TODO: measure actual latency
}

// ── Action Server ────────────────────────────────────────────

rclcpp_action::GoalResponse MotionStreamerNode::handle_execute_goal(
  const rclcpp_action::GoalUUID &,
  std::shared_ptr<const ms_msgs::action::ExecuteTeleop::Goal> goal)
{
  RCLCPP_INFO(this->get_logger(), "ExecuteTeleop goal: device=%s type=%s timeout=%ds",
              goal->device_id.c_str(), goal->device_type.c_str(), goal->timeout_sec);

  if (current_state_ != State::IDLE && current_state_ != State::ERROR) {
    RCLCPP_WARN(this->get_logger(), "Rejecting goal — already in session");
    return rclcpp_action::GoalResponse::REJECT;
  }
  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse MotionStreamerNode::handle_execute_cancel(
  const std::shared_ptr<ExecuteTeleopGoalHandle>)
{
  RCLCPP_INFO(this->get_logger(), "ExecuteTeleop cancel received");
  return rclcpp_action::CancelResponse::ACCEPT;
}

void MotionStreamerNode::handle_execute_accepted(
  const std::shared_ptr<ExecuteTeleopGoalHandle> goal_handle)
{
  std::thread{std::bind(&MotionStreamerNode::execute_teleop_session, this,
                        std::placeholders::_1),
              goal_handle}
    .detach();
}

void MotionStreamerNode::execute_teleop_session(
  const std::shared_ptr<ExecuteTeleopGoalHandle> goal_handle)
{
  const auto goal = goal_handle->get_goal();

  // Phase 1: Connect
  if (!connect_device(goal->device_id, goal->device_type)) {
    auto result = std::make_shared<ms_msgs::action::ExecuteTeleop::Result>();
    result->success = false;
    result->error_code = 2;
    result->message = "Device connection failed";
    goal_handle->abort(result);
    return;
  }

  retargeting_config_ = goal->config;
  session_id_ = goal->device_id + "_" + std::to_string(this->now().nanoseconds());
  transition_to(State::READY);

  // Phase 2: Stream
  transition_to(State::STREAMING);
  rclcpp::Time start_time = this->now();
  rclcpp::Duration timeout = rclcpp::Duration::from_seconds(goal->timeout_sec);

  while (rclcpp::ok() &&
         current_state_ != State::ERROR &&
         current_state_ != State::IDLE)
  {
    if (goal_handle->is_canceling()) {
      auto result = std::make_shared<ms_msgs::action::ExecuteTeleop::Result>();
      result->success = false;
      result->error_code = 0;
      result->message = "Canceled by client";
      goal_handle->canceled(result);
      disconnect_device();
      session_id_.clear();
      transition_to(State::IDLE);
      return;
    }

    // Check timeout
    if (goal->timeout_sec > 0 && (this->now() - start_time) > timeout) {
      RCLCPP_INFO(this->get_logger(), "Teleop session timeout reached");
      break;
    }

    // Publish feedback
    auto feedback = std::make_shared<ms_msgs::action::ExecuteTeleop::Feedback>();
    feedback->state = static_cast<uint8_t>(current_state_);
    feedback->progress_percent = std::min(100.0f,
      static_cast<float>((this->now() - start_time).seconds() / std::max(1, goal->timeout_sec) * 100.0f));
    feedback->tracking_quality = tracking_quality_;
    feedback->frames_processed = frame_count_;
    feedback->current_phase = (current_state_ == State::PAUSED) ? "paused" : "streaming";
    goal_handle->publish_feedback(feedback);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Phase 3: Cleanup
  disconnect_device();
  session_id_.clear();

  if (current_state_ == State::ERROR) {
    auto result = std::make_shared<ms_msgs::action::ExecuteTeleop::Result>();
    result->success = false;
    result->error_code = 4;  // Streaming error
    result->message = "Streaming error (tracking lost or device failure)";
    goal_handle->abort(result);
    transition_to(State::IDLE);
    return;
  }

  auto result = std::make_shared<ms_msgs::action::ExecuteTeleop::Result>();
  result->success = true;
  result->error_code = 0;
  result->message = "Teleop session completed";
  rclcpp::Duration elapsed = this->now() - start_time;
  result->actual_duration.sec = static_cast<int32_t>(elapsed.seconds());
  result->actual_duration.nanosec = static_cast<uint32_t>(elapsed.nanoseconds() % 1000000000LL);
  goal_handle->succeed(result);
  transition_to(State::IDLE);
}

}  // namespace ms

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(ms::MotionStreamerNode)
