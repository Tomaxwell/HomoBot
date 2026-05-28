#include "interaction/interaction_node.hpp"

#include <chrono>

namespace interaction
{

InteractionNode::InteractionNode(const rclcpp::NodeOptions &options)
: Node("interaction_node", options)
{
  this->declare_parameter("heartbeat_rate_hz", 1.0);
  this->declare_parameter("max_history_size", 10000);
  this->declare_parameter("min_confidence_threshold", 0.6);

  heartbeat_rate_hz_ = this->get_parameter("heartbeat_rate_hz").as_double();
  max_history_size_ = this->get_parameter("max_history_size").as_int();
  min_confidence_threshold_ = static_cast<float>(this->get_parameter("min_confidence_threshold").as_double());

  uptime_start_ = this->now();

  // Publishers
  state_pub_ = this->create_publisher<interaction_msgs::msg::InteractionState>(
    "/interaction/interaction_state", rclcpp::QoS(1).transient_local().reliable());

  event_pub_ = this->create_publisher<interaction_msgs::msg::InteractionEvent>(
    "/interaction/interaction_event", rclcpp::QoS(100));

  voice_intent_pub_ = this->create_publisher<interaction_msgs::msg::VoiceIntent>(
    "/interaction/voice_intent", rclcpp::QoS(10));

  visual_intent_pub_ = this->create_publisher<interaction_msgs::msg::VisualIntent>(
    "/interaction/visual_intent", rclcpp::QoS(10));

  touch_intent_pub_ = this->create_publisher<interaction_msgs::msg::TouchIntent>(
    "/interaction/touch_intent", rclcpp::QoS(10));

  emotion_pub_ = this->create_publisher<interaction_msgs::msg::EmotionState>(
    "/interaction/emotion_state", rclcpp::QoS(1));

  heartbeat_pub_ = this->create_publisher<interaction_msgs::msg::Heartbeat>(
    "/interaction/heartbeat", rclcpp::QoS(1));

  // Subscribers
  asr_sub_ = this->create_subscription<hal_audio_msgs::msg::SpeechRecognitionResult>(
    "/hal_audio/speech_recognition_result", rclcpp::QoS(10),
    std::bind(&InteractionNode::on_asr_result, this, std::placeholders::_1));

  vad_sub_ = this->create_subscription<hal_audio_msgs::msg::VadEvent>(
    "/hal_audio/vad_event", rclcpp::QoS(10),
    std::bind(&InteractionNode::on_vad_event, this, std::placeholders::_1));

  // Service servers
  get_health_status_srv_ = this->create_service<interaction_msgs::srv::GetHealthStatus>(
    "/interaction/get_health_status",
    std::bind(&InteractionNode::handle_get_health_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  query_history_srv_ = this->create_service<interaction_msgs::srv::QueryInteractionHistory>(
    "/interaction/query_interaction_history",
    std::bind(&InteractionNode::handle_query_interaction_history, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  set_mode_srv_ = this->create_service<interaction_msgs::srv::SetInteractionMode>(
    "/interaction/set_interaction_mode",
    std::bind(&InteractionNode::handle_set_interaction_mode, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  send_message_srv_ = this->create_service<interaction_msgs::srv::SendMessage>(
    "/interaction/send_message",
    std::bind(&InteractionNode::handle_send_message, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Timer
  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / heartbeat_rate_hz_),
    std::bind(&InteractionNode::publish_heartbeat, this));

  transition_to(State::STANDBY);
  RCLCPP_INFO(this->get_logger(), "Interaction node initialized");
}

InteractionNode::~InteractionNode()
{
  RCLCPP_INFO(this->get_logger(), "Interaction node shutting down");
}

void InteractionNode::publish_state()
{
  auto msg = interaction_msgs::msg::InteractionState();
  msg.state = static_cast<uint8_t>(current_state_);
  msg.attention_level = 0.0f;
  msg.dialogue_turn = 0;
  state_pub_->publish(msg);
}

void InteractionNode::publish_heartbeat()
{
  auto msg = interaction_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.node_name = this->get_name();
  msg.state = static_cast<uint8_t>(current_state_);
  heartbeat_pub_->publish(msg);
}

void InteractionNode::transition_to(State new_state)
{
  if (current_state_ != new_state) {
    current_state_ = new_state;
    publish_state();
  }
}

void InteractionNode::on_asr_result(const hal_audio_msgs::msg::SpeechRecognitionResult::SharedPtr msg)
{
  if (msg->is_wake_word) {
    transition_to(State::LISTENING);
    RCLCPP_INFO(this->get_logger(), "Wake word detected: %s", msg->wake_word.c_str());
    return;
  }

  if (!msg->is_final) {
    return;
  }

  // Create voice intent
  auto voice = interaction_msgs::msg::VoiceIntent();
  voice.stamp = this->now();
  voice.text = msg->text;
  voice.language = msg->language;
  voice.confidence = msg->confidence;
  voice.is_wake_word = false;
  voice.is_command = true;
  voice_intent_pub_->publish(voice);

  // Create interaction event
  auto event = interaction_msgs::msg::InteractionEvent();
  event.stamp = this->now();
  event.event_id = "evt_" + std::to_string(this->now().nanoseconds());
  event.modality = interaction_msgs::msg::InteractionEvent::MODALITY_VOICE;
  event.intent_category = "command";
  event.text = msg->text;
  event.confidence = msg->confidence;
  event_pub_->publish(event);

  // Store in history
  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    event_history_.push_back(event);
    if (event_history_.size() > max_history_size_) {
      event_history_.pop_front();
    }
  }

  transition_to(State::PROCESSING);
  // TODO: Intent understanding and arbitration
  transition_to(State::RESPONDING);
  // TODO: Send response
  transition_to(State::STANDBY);
}

void InteractionNode::on_vad_event(const hal_audio_msgs::msg::VadEvent::SharedPtr msg)
{
  if (msg->is_speech && current_state_ == State::STANDBY) {
    transition_to(State::LISTENING);
  }
}

void InteractionNode::handle_get_health_status(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<interaction_msgs::srv::GetHealthStatus::Request>,
  std::shared_ptr<interaction_msgs::srv::GetHealthStatus::Response> response)
{
  response->healthy = current_state_ != State::FAULT;
  response->node_name = this->get_name();
  response->state = static_cast<uint8_t>(current_state_);
}

void InteractionNode::handle_query_interaction_history(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<interaction_msgs::srv::QueryInteractionHistory::Request> request,
  std::shared_ptr<interaction_msgs::srv::QueryInteractionHistory::Response> response)
{
  std::lock_guard<std::mutex> lock(history_mutex_);
  response->events.clear();

  for (const auto &event : event_history_) {
    auto t = rclcpp::Time(event.stamp);
    if (t >= rclcpp::Time(request->start_time) && t <= rclcpp::Time(request->end_time)) {
      if (request->user_id_filter.empty() || event.source_user_id == request->user_id_filter) {
        if (request->modality_filter == 255 || event.modality == request->modality_filter) {
          response->events.push_back(event);
        }
      }
    }
  }

  response->success = true;
  response->count = response->events.size();
}

void InteractionNode::handle_set_interaction_mode(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<interaction_msgs::srv::SetInteractionMode::Request> request,
  std::shared_ptr<interaction_msgs::srv::SetInteractionMode::Response> response)
{
  current_mode_ = request->mode;
  response->success = true;
  response->error_code = 0;
  response->message = "Mode set to " + std::to_string(request->mode);
}

void InteractionNode::handle_send_message(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<interaction_msgs::srv::SendMessage::Request> request,
  std::shared_ptr<interaction_msgs::srv::SendMessage::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "Sending message to %s: %s",
              request->target_user_id.c_str(), request->text.c_str());
  response->success = true;
  response->error_code = 0;
  response->message_id = "msg_" + std::to_string(this->now().nanoseconds());
}

}  // namespace interaction

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(interaction::InteractionNode)
