#ifndef INTERACTION__INTERACTION_NODE_HPP_
#define INTERACTION__INTERACTION_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <map>
#include <vector>
#include <deque>

#include "interaction_msgs/msg/emotion_state.hpp"
#include "interaction_msgs/msg/heartbeat.hpp"
#include "interaction_msgs/msg/interaction_event.hpp"
#include "interaction_msgs/msg/interaction_state.hpp"
#include "interaction_msgs/msg/message_intent.hpp"
#include "interaction_msgs/msg/touch_intent.hpp"
#include "interaction_msgs/msg/visual_intent.hpp"
#include "interaction_msgs/msg/voice_intent.hpp"
#include "interaction_msgs/srv/get_health_status.hpp"
#include "interaction_msgs/srv/query_interaction_history.hpp"
#include "interaction_msgs/srv/send_message.hpp"
#include "interaction_msgs/srv/set_interaction_mode.hpp"

namespace interaction
{

enum class State : uint8_t
{
  IDLE = 0,
  STANDBY = 1,
  LISTENING = 2,
  PROCESSING = 3,
  RESPONDING = 4,
  DELEGATING = 5,
  AWAITING_CONFIRM = 6,
  FAULT = 7
};

class InteractionNode : public rclcpp::Node
{
public:
  explicit InteractionNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~InteractionNode() override;

private:
  void publish_state();
  void publish_heartbeat();
  void transition_to(State new_state);

  // Input handlers
  void on_asr_result(const hal_audio_msgs::msg::SpeechRecognitionResult::SharedPtr msg);
  void on_vad_event(const hal_audio_msgs::msg::VadEvent::SharedPtr msg);

  // Service handlers
  void handle_get_health_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<interaction_msgs::srv::GetHealthStatus::Request> request,
    std::shared_ptr<interaction_msgs::srv::GetHealthStatus::Response> response);

  void handle_query_interaction_history(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<interaction_msgs::srv::QueryInteractionHistory::Request> request,
    std::shared_ptr<interaction_msgs::srv::QueryInteractionHistory::Response> response);

  void handle_set_interaction_mode(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<interaction_msgs::srv::SetInteractionMode::Request> request,
    std::shared_ptr<interaction_msgs::srv::SetInteractionMode::Response> response);

  void handle_send_message(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<interaction_msgs::srv::SendMessage::Request> request,
    std::shared_ptr<interaction_msgs::srv::SendMessage::Response> response);

  // Publishers
  rclcpp::Publisher<interaction_msgs::msg::InteractionState>::SharedPtr state_pub_;
  rclcpp::Publisher<interaction_msgs::msg::InteractionEvent>::SharedPtr event_pub_;
  rclcpp::Publisher<interaction_msgs::msg::VoiceIntent>::SharedPtr voice_intent_pub_;
  rclcpp::Publisher<interaction_msgs::msg::VisualIntent>::SharedPtr visual_intent_pub_;
  rclcpp::Publisher<interaction_msgs::msg::TouchIntent>::SharedPtr touch_intent_pub_;
  rclcpp::Publisher<interaction_msgs::msg::EmotionState>::SharedPtr emotion_pub_;
  rclcpp::Publisher<interaction_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Subscribers
  rclcpp::Subscription<hal_audio_msgs::msg::SpeechRecognitionResult>::SharedPtr asr_sub_;
  rclcpp::Subscription<hal_audio_msgs::msg::VadEvent>::SharedPtr vad_sub_;

  // Service servers
  rclcpp::Service<interaction_msgs::srv::GetHealthStatus>::SharedPtr get_health_status_srv_;
  rclcpp::Service<interaction_msgs::srv::QueryInteractionHistory>::SharedPtr query_history_srv_;
  rclcpp::Service<interaction_msgs::srv::SetInteractionMode>::SharedPtr set_mode_srv_;
  rclcpp::Service<interaction_msgs::srv::SendMessage>::SharedPtr send_message_srv_;

  // Timers
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // State
  State current_state_{State::IDLE};
  rclcpp::Time uptime_start_;
  uint8_t current_mode_{0};

  // History
  std::deque<interaction_msgs::msg::InteractionEvent> event_history_;
  std::mutex history_mutex_;

  // Parameters
  double heartbeat_rate_hz_{1.0};
  size_t max_history_size_{10000};
  float min_confidence_threshold_{0.6f};
};

}  // namespace interaction

#endif  // INTERACTION__INTERACTION_NODE_HPP_
