#ifndef MS__MS_NODE_HPP_
#define MS__MS_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "ms_msgs/msg/teleop_state.hpp"
#include "ms_msgs/msg/device_state.hpp"
#include "mc_msgs/msg/motion_target.hpp"
#include "ms_msgs/msg/retargeting_config.hpp"
#include "ms_msgs/msg/heartbeat.hpp"
#include "ms_msgs/srv/start_teleop.hpp"
#include "ms_msgs/srv/stop_teleop.hpp"
#include "ms_msgs/srv/pause_teleop.hpp"
#include "ms_msgs/srv/get_device_status.hpp"
#include "ms_msgs/srv/get_health_status.hpp"
#include "ms_msgs/action/execute_teleop.hpp"

namespace ms
{

class MotionStreamerNode : public rclcpp::Node
{
public:
  explicit MotionStreamerNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~MotionStreamerNode() override;

private:
  enum class State : uint8_t
  {
    IDLE = 0,
    CONNECTING = 1,
    READY = 2,
    STREAMING = 3,
    PAUSED = 4,
    ERROR = 5
  };

  // ── Lifecycle ──
  void transition_to(State new_state);
  bool is_motion_allowed() const;

  // ── Device I/O (stubs — replace with actual VR/mocap drivers) ──
  bool connect_device(const std::string &device_id, const std::string &device_type);
  void disconnect_device();
  bool poll_device_data();
  float get_device_tracking_quality() const;

  // ── Retargeting (stub — replace with actual retargeting engine) ──
  mc_msgs::msg::MotionTarget retarget_human_pose();

  // ── Timer callbacks ──
  void on_device_poll();
  void on_publish_teleop_state();
  void on_publish_device_state();
  void on_publish_heartbeat();

  // ── Service handlers ──
  void handle_start_teleop(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::StartTeleop::Request> request,
    std::shared_ptr<ms_msgs::srv::StartTeleop::Response> response);

  void handle_stop_teleop(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::StopTeleop::Request> request,
    std::shared_ptr<ms_msgs::srv::StopTeleop::Response> response);

  void handle_pause_teleop(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::PauseTeleop::Request> request,
    std::shared_ptr<ms_msgs::srv::PauseTeleop::Response> response);

  void handle_get_device_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::GetDeviceStatus::Request> request,
    std::shared_ptr<ms_msgs::srv::GetDeviceStatus::Response> response);

  void handle_get_health_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::GetHealthStatus::Request> request,
    std::shared_ptr<ms_msgs::srv::GetHealthStatus::Response> response);

  // ── Action server (ExecuteTeleop) ──
  using ExecuteTeleopGoalHandle = rclcpp_action::ServerGoalHandle<ms_msgs::action::ExecuteTeleop>;

  rclcpp_action::GoalResponse handle_execute_goal(
    const rclcpp_action::GoalUUID &uuid,
    std::shared_ptr<const ms_msgs::action::ExecuteTeleop::Goal> goal);

  rclcpp_action::CancelResponse handle_execute_cancel(
    const std::shared_ptr<ExecuteTeleopGoalHandle> goal_handle);

  void handle_execute_accepted(const std::shared_ptr<ExecuteTeleopGoalHandle> goal_handle);
  void execute_teleop_session(const std::shared_ptr<ExecuteTeleopGoalHandle> goal_handle);

  // ── Publishers ──
  rclcpp::Publisher<mc_msgs::msg::MotionTarget>::SharedPtr motion_target_pub_;
  rclcpp::Publisher<ms_msgs::msg::TeleopState>::SharedPtr teleop_state_pub_;
  rclcpp::Publisher<ms_msgs::msg::DeviceState>::SharedPtr device_state_pub_;
  rclcpp::Publisher<ms_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // ── Service servers ──
  rclcpp::Service<ms_msgs::srv::StartTeleop>::SharedPtr start_teleop_srv_;
  rclcpp::Service<ms_msgs::srv::StopTeleop>::SharedPtr stop_teleop_srv_;
  rclcpp::Service<ms_msgs::srv::PauseTeleop>::SharedPtr pause_teleop_srv_;
  rclcpp::Service<ms_msgs::srv::GetDeviceStatus>::SharedPtr get_device_status_srv_;
  rclcpp::Service<ms_msgs::srv::GetHealthStatus>::SharedPtr get_health_status_srv_;

  // ── Action server ──
  rclcpp_action::Server<ms_msgs::action::ExecuteTeleop>::SharedPtr execute_teleop_srv_;

  // ── Timers ──
  rclcpp::TimerBase::SharedPtr device_poll_timer_;
  rclcpp::TimerBase::SharedPtr teleop_state_timer_;
  rclcpp::TimerBase::SharedPtr device_state_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // ── State ──
  std::mutex state_mutex_;
  std::atomic<State> current_state_{State::IDLE};
  std::atomic<State> prev_state_{State::IDLE};
  builtin_interfaces::msg::Time state_changed_at_;
  rclcpp::Time uptime_start_;

  // ── Session info ──
  std::string session_id_;
  std::string device_id_;
  std::string device_type_;
  ms_msgs::msg::RetargetingConfig retargeting_config_;
  uint32_t frame_count_{0};
  float tracking_quality_{0.0f};

  // ── Parameters ──
  double device_poll_rate_hz_{60.0};
  double teleop_state_rate_hz_{10.0};
  double device_state_rate_hz_{10.0};
  double heartbeat_rate_hz_{1.0};
  float tracking_quality_threshold_{0.5f};
  uint32_t max_dropped_frames_{30};
};

}  // namespace ms

#endif  // MS__MS_NODE_HPP_
