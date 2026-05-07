#ifndef HAL_AUDIO__HAL_AUDIO_NODE_HPP_
#define HAL_AUDIO__HAL_AUDIO_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include "hal_audio_msgs/action/speak.hpp"
#include "hal_audio_msgs/msg/audio_device_state.hpp"
#include "hal_audio_msgs/msg/audio_frame.hpp"
#include "hal_audio_msgs/msg/heartbeat.hpp"
#include "hal_audio_msgs/msg/speech_recognition_result.hpp"
#include "hal_audio_msgs/msg/tts_request.hpp"
#include "hal_audio_msgs/msg/vad_event.hpp"
#include "hal_audio_msgs/srv/get_health_status.hpp"
#include "hal_audio_msgs/srv/set_audio_parameters.hpp"
#include "hal_audio_msgs/srv/start_recording.hpp"
#include "hal_audio_msgs/srv/stop_recording.hpp"

namespace hal_audio
{

enum class State : uint8_t
{
  IDLE = 0,
  INITIALIZING = 1,
  READY = 2,
  RECORDING = 3,
  PLAYING = 4,
  FAULT = 5
};

class HALAudioNode : public rclcpp::Node
{
public:
  explicit HALAudioNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~HALAudioNode() override;

private:
  void publish_state();
  void publish_heartbeat();
  void transition_to(State new_state);
  void on_tts_request(const hal_audio_msgs::msg::TtsRequest::SharedPtr msg);
  void on_sm_state(const sm_msgs::msg::RobotState::SharedPtr msg);

  // Action server
  rclcpp_action::Server<hal_audio_msgs::action::Speak>::SharedPtr speak_server_;

  rclcpp_action::GoalResponse handle_speak_goal(
    const rclcpp_action::GoalUUID &uuid,
    std::shared_ptr<const hal_audio_msgs::action::Speak::Goal> goal);

  rclcpp_action::CancelResponse handle_speak_cancel(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<hal_audio_msgs::action::Speak>> goal_handle);

  void handle_speak_accepted(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<hal_audio_msgs::action::Speak>> goal_handle);

  void execute_speak(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<hal_audio_msgs::action::Speak>> goal_handle);

  // Service handlers
  void handle_get_health_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<hal_audio_msgs::srv::GetHealthStatus::Request> request,
    std::shared_ptr<hal_audio_msgs::srv::GetHealthStatus::Response> response);

  void handle_start_recording(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<hal_audio_msgs::srv::StartRecording::Request> request,
    std::shared_ptr<hal_audio_msgs::srv::StartRecording::Response> response);

  void handle_stop_recording(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<hal_audio_msgs::srv::StopRecording::Request> request,
    std::shared_ptr<hal_audio_msgs::srv::StopRecording::Response> response);

  void handle_set_audio_parameters(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<hal_audio_msgs::srv::SetAudioParameters::Request> request,
    std::shared_ptr<hal_audio_msgs::srv::SetAudioParameters::Response> response);

  // Publishers
  rclcpp::Publisher<hal_audio_msgs::msg::AudioDeviceState>::SharedPtr state_pub_;
  rclcpp::Publisher<hal_audio_msgs::msg::VadEvent>::SharedPtr vad_pub_;
  rclcpp::Publisher<hal_audio_msgs::msg::SpeechRecognitionResult>::SharedPtr asr_pub_;
  rclcpp::Publisher<hal_audio_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Subscribers
  rclcpp::Subscription<hal_audio_msgs::msg::TtsRequest>::SharedPtr tts_request_sub_;
  rclcpp::Subscription<sm_msgs::msg::RobotState>::SharedPtr sm_state_sub_;

  // Service servers
  rclcpp::Service<hal_audio_msgs::srv::GetHealthStatus>::SharedPtr get_health_status_srv_;
  rclcpp::Service<hal_audio_msgs::srv::StartRecording>::SharedPtr start_recording_srv_;
  rclcpp::Service<hal_audio_msgs::srv::StopRecording>::SharedPtr stop_recording_srv_;
  rclcpp::Service<hal_audio_msgs::srv::SetAudioParameters>::SharedPtr set_audio_parameters_srv_;

  // Timers
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // State
  State current_state_{State::IDLE};
  State prev_state_{State::IDLE};
  builtin_interfaces::msg::Time state_changed_at_;
  rclcpp::Time uptime_start_;
  bool is_recording_{false};
  bool is_playing_{false};

  // Parameters
  double heartbeat_rate_hz_{1.0};
  std::string audio_device_name_;
  uint32_t sample_rate_hz_{16000};
  uint8_t channels_{4};
  float input_gain_db_{20.0f};
  float output_volume_{0.8f};
  bool enable_aec_{true};
  bool enable_ns_{true};
};

}  // namespace hal_audio

#endif  // HAL_AUDIO__HAL_AUDIO_NODE_HPP_
