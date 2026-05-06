#include "mp/mp_node.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace mp
{

MotionPlayerNode::MotionPlayerNode(const rclcpp::NodeOptions & options)
: Node("mp_node", options)
{
  // Parameters
  this->declare_parameter("heartbeat_rate_hz", 1.0);
  this->declare_parameter("motion_library_path", "/opt/striding/motions");

  heartbeat_rate_hz_ = this->get_parameter("heartbeat_rate_hz").as_double();
  motion_library_path_ = this->get_parameter("motion_library_path").as_string();

  uptime_start_ = this->now();
  state_changed_at_ = this->now();

  // Publishers
  state_pub_ = this->create_publisher<mp_msgs::msg::MotionPlayerState>(
    "/mp/motion_player_state", rclcpp::QoS(1).transient_local().reliable());

  heartbeat_pub_ = this->create_publisher<mp_msgs::msg::Heartbeat>(
    "/mp/heartbeat", rclcpp::QoS(1));

  // Action server
  play_motion_server_ = rclcpp_action::create_server<mp_msgs::action::PlayMotion>(
    this,
    "/mp/play_motion",
    std::bind(&MotionPlayerNode::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
    std::bind(&MotionPlayerNode::handle_cancel, this, std::placeholders::_1),
    std::bind(&MotionPlayerNode::handle_accepted, this, std::placeholders::_1));

  // Service servers
  get_health_status_srv_ = this->create_service<mp_msgs::srv::GetHealthStatus>(
    "/mp/get_health_status",
    std::bind(&MotionPlayerNode::handle_get_health_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_motion_catalog_srv_ = this->create_service<mp_msgs::srv::GetMotionCatalog>(
    "/mp/get_motion_catalog",
    std::bind(&MotionPlayerNode::handle_get_motion_catalog, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_motion_info_srv_ = this->create_service<mp_msgs::srv::GetMotionInfo>(
    "/mp/get_motion_info",
    std::bind(&MotionPlayerNode::handle_get_motion_info, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  stop_motion_srv_ = this->create_service<mp_msgs::srv::StopMotion>(
    "/mp/stop_motion",
    std::bind(&MotionPlayerNode::handle_stop_motion, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Timer
  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / heartbeat_rate_hz_),
    std::bind(&MotionPlayerNode::publish_heartbeat, this));

  RCLCPP_INFO(this->get_logger(), "MotionPlayer node initialized");
}

MotionPlayerNode::~MotionPlayerNode()
{
  RCLCPP_INFO(this->get_logger(), "MotionPlayer node shutting down");
}

void MotionPlayerNode::publish_state()
{
  auto msg = mp_msgs::msg::MotionPlayerState();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.prev_state = static_cast<uint8_t>(prev_state_);
  msg.state_changed_at = state_changed_at_;
  state_pub_->publish(msg);
}

void MotionPlayerNode::publish_heartbeat()
{
  auto msg = mp_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.node_name = this->get_name();
  msg.state = static_cast<uint8_t>(current_state_);
  heartbeat_pub_->publish(msg);
}

void MotionPlayerNode::transition_to(State new_state)
{
  if (current_state_ != new_state) {
    prev_state_ = current_state_;
    current_state_ = new_state;
    state_changed_at_ = this->now();
    publish_state();
  }
}

bool MotionPlayerNode::is_motion_allowed()
{
  // TODO: Query SM via /sm/is_motion_allowed service
  // For now, assume motion is allowed if not in FAULT state
  return current_state_ != State::FAULT;
}

rclcpp_action::GoalResponse MotionPlayerNode::handle_goal(
  const rclcpp_action::GoalUUID & /*uuid*/,
  std::shared_ptr<const mp_msgs::action::PlayMotion::Goal> goal)
{
  RCLCPP_INFO(this->get_logger(), "Received play motion request: %s", goal->motion_id.c_str());

  if (!is_motion_allowed()) {
    RCLCPP_WARN(this->get_logger(), "Motion not allowed in current state");
    return rclcpp_action::GoalResponse::REJECT;
  }

  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse MotionPlayerNode::handle_cancel(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<mp_msgs::action::PlayMotion>> /*goal_handle*/)
{
  RCLCPP_INFO(this->get_logger(), "Received cancel request");
  return rclcpp_action::CancelResponse::ACCEPT;
}

void MotionPlayerNode::handle_accepted(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<mp_msgs::action::PlayMotion>> goal_handle)
{
  std::thread{std::bind(&MotionPlayerNode::execute_motion, this, std::placeholders::_1), goal_handle}.detach();
}

void MotionPlayerNode::execute_motion(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<mp_msgs::action::PlayMotion>> goal_handle)
{
  const auto goal = goal_handle->get_goal();
  auto result = std::make_shared<mp_msgs::action::PlayMotion::Result>();

  {
    std::lock_guard<std::mutex> lock(goal_mutex_);
    current_goal_handle_ = goal_handle;
  }

  transition_to(State::PLAYING);

  // TODO: Load motion from library and execute frame by frame
  // For skeleton implementation, simulate a short motion
  rclcpp::Rate rate(100);  // 100 Hz
  for (uint32_t frame = 0; frame < 100; ++frame) {
    if (goal_handle->is_canceling()) {
      result->success = false;
      result->error_code = 0;
      result->message = "Canceled by user";
      goal_handle->canceled(result);
      transition_to(State::IDLE);
      return;
    }

    auto feedback = std::make_shared<mp_msgs::action::PlayMotion::Feedback>();
    feedback->state = static_cast<uint8_t>(current_state_);
    feedback->progress_percent = static_cast<float>(frame) / 100.0f * 100.0f;
    feedback->current_frame = frame;
    feedback->total_frames = 100;
    feedback->current_phase = "playing";
    goal_handle->publish_feedback(feedback);

    rate.sleep();
  }

  result->success = true;
  result->error_code = 0;
  result->message = "Motion completed";
  result->frames_played = 100;
  goal_handle->succeed(result);

  {
    std::lock_guard<std::mutex> lock(goal_mutex_);
    current_goal_handle_.reset();
  }

  transition_to(State::IDLE);
}

void MotionPlayerNode::handle_get_health_status(
  const std::shared_ptr<rmw_request_id_t> /*request_header*/,
  const std::shared_ptr<mp_msgs::srv::GetHealthStatus::Request> /*request*/,
  std::shared_ptr<mp_msgs::srv::GetHealthStatus::Response> response)
{
  response->healthy = current_state_ != State::FAULT;
  response->node_name = this->get_name();
  response->uptime_since = builtin_interfaces::msg::Time()
    .set__sec(static_cast<int32_t>(uptime_start_.seconds()))
    .set__nanosec(static_cast<uint32_t>((uptime_start_.nanoseconds() % 1000000000)));
  response->state = static_cast<uint8_t>(current_state_);
}

void MotionPlayerNode::handle_get_motion_catalog(
  const std::shared_ptr<rmw_request_id_t> /*request_header*/,
  const std::shared_ptr<mp_msgs::srv::GetMotionCatalog::Request> /*request*/,
  std::shared_ptr<mp_msgs::srv::GetMotionCatalog::Response> response)
{
  response->success = true;
  response->motions = motion_catalog_;
  response->count = motion_catalog_.size();
}

void MotionPlayerNode::handle_get_motion_info(
  const std::shared_ptr<rmw_request_id_t> /*request_header*/,
  const std::shared_ptr<mp_msgs::srv::GetMotionInfo::Request> request,
  std::shared_ptr<mp_msgs::srv::GetMotionInfo::Response> response)
{
  response->success = false;
  for (const auto & motion : motion_catalog_) {
    if (motion.motion_id == request->motion_id) {
      response->success = true;
      response->info = motion;
      break;
    }
  }
}

void MotionPlayerNode::handle_stop_motion(
  const std::shared_ptr<rmw_request_id_t> /*request_header*/,
  const std::shared_ptr<mp_msgs::srv::StopMotion::Request> request,
  std::shared_ptr<mp_msgs::srv::StopMotion::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "Stop motion requested (emergency=%d)", request->emergency);

  {
    std::lock_guard<std::mutex> lock(goal_mutex_);
    if (current_goal_handle_ && current_goal_handle_->is_active()) {
      current_goal_handle_->abort(nullptr);
      current_goal_handle_.reset();
    }
  }

  transition_to(State::IDLE);
  response->stopped = true;
  response->error_code = 0;
  response->message = "Motion stopped";
}

}  // namespace mp

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(mp::MotionPlayerNode)
