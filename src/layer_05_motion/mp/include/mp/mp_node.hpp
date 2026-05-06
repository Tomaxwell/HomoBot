#ifndef MP__MP_NODE_HPP_
#define MP__MP_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include "mp_msgs/action/play_motion.hpp"
#include "mp_msgs/msg/heartbeat.hpp"
#include "mp_msgs/msg/motion_catalog.hpp"
#include "mp_msgs/msg/motion_frame.hpp"
#include "mp_msgs/msg/motion_player_state.hpp"
#include "mp_msgs/srv/get_health_status.hpp"
#include "mp_msgs/srv/get_motion_catalog.hpp"
#include "mp_msgs/srv/get_motion_info.hpp"
#include "mp_msgs/srv/stop_motion.hpp"

namespace mp
{

class MotionPlayerNode : public rclcpp::Node
{
public:
  explicit MotionPlayerNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~MotionPlayerNode() override;

private:
  // State machine
  enum class State : uint8_t
  {
    IDLE = 0,
    LOADING = 1,
    PLAYING = 2,
    PAUSED = 3,
    STOPPING = 4,
    FAULT = 5
  };

  void publish_state();
  void publish_heartbeat();
  void transition_to(State new_state);
  bool is_motion_allowed();

  // Action server
  rclcpp_action::Server<mp_msgs::action::PlayMotion>::SharedPtr play_motion_server_;

  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const mp_msgs::action::PlayMotion::Goal> goal);

  rclcpp_action::CancelResponse handle_cancel(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<mp_msgs::action::PlayMotion>> goal_handle);

  void handle_accepted(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<mp_msgs::action::PlayMotion>> goal_handle);

  void execute_motion(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<mp_msgs::action::PlayMotion>> goal_handle);

  // Service servers
  rclcpp::Service<mp_msgs::srv::GetHealthStatus>::SharedPtr get_health_status_srv_;
  rclcpp::Service<mp_msgs::srv::GetMotionCatalog>::SharedPtr get_motion_catalog_srv_;
  rclcpp::Service<mp_msgs::srv::GetMotionInfo>::SharedPtr get_motion_info_srv_;
  rclcpp::Service<mp_msgs::srv::StopMotion>::SharedPtr stop_motion_srv_;

  void handle_get_health_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<mp_msgs::srv::GetHealthStatus::Request> request,
    std::shared_ptr<mp_msgs::srv::GetHealthStatus::Response> response);

  void handle_get_motion_catalog(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<mp_msgs::srv::GetMotionCatalog::Request> request,
    std::shared_ptr<mp_msgs::srv::GetMotionCatalog::Response> response);

  void handle_get_motion_info(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<mp_msgs::srv::GetMotionInfo::Request> request,
    std::shared_ptr<mp_msgs::srv::GetMotionInfo::Response> response);

  void handle_stop_motion(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<mp_msgs::srv::StopMotion::Request> request,
    std::shared_ptr<mp_msgs::srv::StopMotion::Response> response);

  // Publishers
  rclcpp::Publisher<mp_msgs::msg::MotionPlayerState>::SharedPtr state_pub_;
  rclcpp::Publisher<mp_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Timer
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // State
  State current_state_{State::IDLE};
  State prev_state_{State::IDLE};
  builtin_interfaces::msg::Time state_changed_at_;
  rclcpp::Time uptime_start_;

  // Motion data
  std::vector<mp_msgs::msg::MotionCatalog> motion_catalog_;
  std::shared_ptr<rclcpp_action::ServerGoalHandle<mp_msgs::action::PlayMotion>> current_goal_handle_;
  std::mutex goal_mutex_;

  // Parameters
  double heartbeat_rate_hz_{1.0};
  std::string motion_library_path_;
};

}  // namespace mp

#endif  // MP__MP_NODE_HPP_
