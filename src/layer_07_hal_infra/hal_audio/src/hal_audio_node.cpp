#include "hal_audio/hal_audio_node.hpp"

#include <chrono>

namespace hal_audio
{

HALAudioNode::HALAudioNode(const rclcpp::NodeOptions &options)
: Node("hal_audio_node", options)
{
  this->declare_parameter("heartbeat_rate_hz", 1.0);
  this->declare_parameter("audio_device_name", "hw:0,0");
  this->declare_parameter("sample_rate_hz", 16000);
  this->declare_parameter("channels", 4);
  this->declare_parameter("input_gain_db", 20.0);
  this->declare_parameter("output_volume", 0.8);
  this->declare_parameter("enable_aec", true);
  this->declare_parameter("enable_ns", true);

  heartbeat_rate_hz_ = this->get_parameter("heartbeat_rate_hz").as_double();
  audio_device_name_ = this->get_parameter("audio_device_name").as_string();
  sample_rate_hz_ = this->get_parameter("sample_rate_hz").as_int();
  channels_ = this->get_parameter("channels").as_int();
  input_gain_db_ = static_cast<float>(this->get_parameter("input_gain_db").as_double());
  output_volume_ = static_cast<float>(this->get_parameter("output_volume").as_double());
  enable_aec_ = this->get_parameter("enable_aec").as_bool();
  enable_ns_ = this->get_parameter("enable_ns").as_bool();

  uptime_start_ = this->now();
  state_changed_at_ = this->now();

  // Publishers
  state_pub_ = this->create_publisher<hal_audio_msgs::msg::AudioDeviceState>(
    "/hal_audio/audio_device_state", rclcpp::QoS(1).transient_local().reliable());

  vad_pub_ = this->create_publisher<hal_audio_msgs::msg::VadEvent>(
    "/hal_audio/vad_event", rclcpp::QoS(10));

  asr_pub_ = this->create_publisher<hal_audio_msgs::msg::SpeechRecognitionResult>(
    "/hal_audio/speech_recognition_result", rclcpp::QoS(10));

  heartbeat_pub_ = this->create_publisher<hal_audio_msgs::msg::Heartbeat>(
    "/hal_audio/heartbeat", rclcpp::QoS(1));

  // Subscribers
  tts_request_sub_ = this->create_subscription<hal_audio_msgs::msg::TtsRequest>(
    "/interaction/tts_request", rclcpp::QoS(10),
    std::bind(&HALAudioNode::on_tts_request, this, std::placeholders::_1));

  // Service servers
  get_health_status_srv_ = this->create_service<hal_audio_msgs::srv::GetHealthStatus>(
    "/hal_audio/get_health_status",
    std::bind(&HALAudioNode::handle_get_health_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  start_recording_srv_ = this->create_service<hal_audio_msgs::srv::StartRecording>(
    "/hal_audio/start_recording",
    std::bind(&HALAudioNode::handle_start_recording, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  stop_recording_srv_ = this->create_service<hal_audio_msgs::srv::StopRecording>(
    "/hal_audio/stop_recording",
    std::bind(&HALAudioNode::handle_stop_recording, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  set_audio_parameters_srv_ = this->create_service<hal_audio_msgs::srv::SetAudioParameters>(
    "/hal_audio/set_audio_parameters",
    std::bind(&HALAudioNode::handle_set_audio_parameters, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Action server
  speak_server_ = rclcpp_action::create_server<hal_audio_msgs::action::Speak>(
    this,
    "/hal_audio/speak",
    std::bind(&HALAudioNode::handle_speak_goal, this, std::placeholders::_1, std::placeholders::_2),
    std::bind(&HALAudioNode::handle_speak_cancel, this, std::placeholders::_1),
    std::bind(&HALAudioNode::handle_speak_accepted, this, std::placeholders::_1));

  // Timer
  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / heartbeat_rate_hz_),
    std::bind(&HALAudioNode::publish_heartbeat, this));

  transition_to(State::INITIALIZING);
  // TODO: Initialize audio hardware
  transition_to(State::READY);

  RCLCPP_INFO(this->get_logger(), "HAL_Audio node initialized");
}

HALAudioNode::~HALAudioNode()
{
  RCLCPP_INFO(this->get_logger(), "HAL_Audio node shutting down");
}

void HALAudioNode::publish_state()
{
  auto msg = hal_audio_msgs::msg::AudioDeviceState();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.prev_state = static_cast<uint8_t>(prev_state_);
  msg.state_changed_at = state_changed_at_;
  msg.mic_available = true;
  msg.speaker_available = true;
  msg.input_gain_db = input_gain_db_;
  msg.output_volume = output_volume_;
  msg.sample_rate_hz = sample_rate_hz_;
  msg.channels = channels_;
  state_pub_->publish(msg);
}

void HALAudioNode::publish_heartbeat()
{
  auto msg = hal_audio_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.node_name = this->get_name();
  msg.state = static_cast<uint8_t>(current_state_);
  heartbeat_pub_->publish(msg);
}

void HALAudioNode::transition_to(State new_state)
{
  if (current_state_ != new_state) {
    prev_state_ = current_state_;
    current_state_ = new_state;
    state_changed_at_ = this->now();
    publish_state();
  }
}

void HALAudioNode::on_tts_request(const hal_audio_msgs::msg::TtsRequest::SharedPtr msg)
{
  RCLCPP_INFO(this->get_logger(), "TTS request: %s", msg->text.c_str());
  // TODO: Trigger TTS synthesis and playback
}

void HALAudioNode::on_sm_state(const sm_msgs::msg::RobotState::SharedPtr msg)
{
  // Stop TTS playback in ACTIVE_E_STOP state
  if (msg->state == 14) {  // ACTIVE_E_STOP
    if (is_playing_) {
      RCLCPP_WARN(this->get_logger(), "E-Stop active, stopping audio playback");
      is_playing_ = false;
    }
  }
}

rclcpp_action::GoalResponse HALAudioNode::handle_speak_goal(
  const rclcpp_action::GoalUUID &,
  std::shared_ptr<const hal_audio_msgs::action::Speak::Goal> goal)
{
  RCLCPP_INFO(this->get_logger(), "Speak request: %s", goal->text.c_str());
  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse HALAudioNode::handle_speak_cancel(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<hal_audio_msgs::action::Speak>>)
{
  return rclcpp_action::CancelResponse::ACCEPT;
}

void HALAudioNode::handle_speak_accepted(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<hal_audio_msgs::action::Speak>> goal_handle)
{
  std::thread{std::bind(&HALAudioNode::execute_speak, this, std::placeholders::_1), goal_handle}.detach();
}

void HALAudioNode::execute_speak(
  const std::shared_ptr<rclcpp_action::ServerGoalHandle<hal_audio_msgs::action::Speak>> goal_handle)
{
  const auto goal = goal_handle->get_goal();
  auto result = std::make_shared<hal_audio_msgs::action::Speak::Result>();

  transition_to(State::PLAYING);
  is_playing_ = true;

  // TODO: Synthesize and play audio
  rclcpp::Rate rate(10);
  for (int i = 0; i < 10; ++i) {
    if (goal_handle->is_canceling()) {
      result->success = false;
      result->error_code = 0;
      result->message = "Canceled";
      goal_handle->canceled(result);
      is_playing_ = false;
      transition_to(State::READY);
      return;
    }

    auto feedback = std::make_shared<hal_audio_msgs::action::Speak::Feedback>();
    feedback->progress_percent = static_cast<float>(i) / 10.0f * 100.0f;
    feedback->is_synthesizing = i < 3;
    feedback->is_playing = i >= 3;
    goal_handle->publish_feedback(feedback);
    rate.sleep();
  }

  result->success = true;
  result->error_code = 0;
  result->message = "Playback completed";
  goal_handle->succeed(result);

  is_playing_ = false;
  transition_to(State::READY);
}

void HALAudioNode::handle_get_health_status(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<hal_audio_msgs::srv::GetHealthStatus::Request>,
  std::shared_ptr<hal_audio_msgs::srv::GetHealthStatus::Response> response)
{
  response->healthy = current_state_ != State::FAULT;
  response->node_name = this->get_name();
  response->state = static_cast<uint8_t>(current_state_);
  response->audio_device_info = audio_device_name_;
}

void HALAudioNode::handle_start_recording(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<hal_audio_msgs::srv::StartRecording::Request>,
  std::shared_ptr<hal_audio_msgs::srv::StartRecording::Response> response)
{
  if (is_recording_) {
    response->success = false;
    response->error_code = 18000;
    response->message = "Already recording";
    return;
  }

  is_recording_ = true;
  transition_to(State::RECORDING);
  response->success = true;
  response->error_code = 0;
  response->message = "Recording started";
}

void HALAudioNode::handle_stop_recording(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<hal_audio_msgs::srv::StopRecording::Request>,
  std::shared_ptr<hal_audio_msgs::srv::StopRecording::Response> response)
{
  if (!is_recording_) {
    response->success = false;
    response->error_code = 18000;
    response->message = "Not recording";
    return;
  }

  is_recording_ = false;
  transition_to(State::READY);
  response->success = true;
  response->error_code = 0;
  response->message = "Recording stopped";
}

void HALAudioNode::handle_set_audio_parameters(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<hal_audio_msgs::srv::SetAudioParameters::Request> request,
  std::shared_ptr<hal_audio_msgs::srv::SetAudioParameters::Response> response)
{
  if (request->input_gain_db < 999.0) {
    input_gain_db_ = request->input_gain_db;
  }
  if (request->output_volume < 999.0) {
    output_volume_ = request->output_volume;
  }
  if (request->sample_rate_hz > 0) {
    sample_rate_hz_ = request->sample_rate_hz;
  }

  response->success = true;
  response->error_code = 0;
  response->message = "Parameters updated";
}

}  // namespace hal_audio

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(hal_audio::HALAudioNode)
