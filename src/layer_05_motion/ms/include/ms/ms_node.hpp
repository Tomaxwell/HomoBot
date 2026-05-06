#ifndef MS__MS_NODE_HPP_
#define MS__MS_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <mutex>
#include <queue>
#include <map>
#include <vector>

#include "ms_msgs/msg/motion_command.hpp"
#include "ms_msgs/msg/motion_target.hpp"
#include "ms_msgs/msg/motion_streamer_state.hpp"
#include "ms_msgs/msg/stream_statistics.hpp"
#include "ms_msgs/msg/heartbeat.hpp"
#include "ms_msgs/srv/get_health_status.hpp"
#include "ms_msgs/srv/set_stream_parameters.hpp"
#include "ms_msgs/srv/stop_stream.hpp"

namespace ms
{

struct CommandEntry
{
  ms_msgs::msg::MotionCommand command;
  rclcpp::Time received_time;
};

class MotionStreamerNode : public rclcpp::Node
{
public:
  explicit MotionStreamerNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~MotionStreamerNode() override;

private:
  enum class State : uint8_t
  {
    IDLE = 0,
    ACTIVE = 1,
    LIMITING = 2,
    FAULT = 3
  };

  void on_motion_command(const ms_msgs::msg::MotionCommand::SharedPtr msg);
  void publish_target();
  void publish_state();
  void publish_statistics();
  void publish_heartbeat();
  void process_buffer();
  void transition_to(State new_state);
  bool is_motion_allowed();

  // Service handlers
  void handle_get_health_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::GetHealthStatus::Request> request,
    std::shared_ptr<ms_msgs::srv::GetHealthStatus::Response> response);

  void handle_set_stream_parameters(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::SetStreamParameters::Request> request,
    std::shared_ptr<ms_msgs::srv::SetStreamParameters::Response> response);

  void handle_stop_stream(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<ms_msgs::srv::StopStream::Request> request,
    std::shared_ptr<ms_msgs::srv::StopStream::Response> response);

  // Subscribers
  rclcpp::Subscription<ms_msgs::msg::MotionCommand>::SharedPtr motion_command_sub_;

  // Publishers
  rclcpp::Publisher<ms_msgs::msg::MotionTarget>::SharedPtr motion_target_pub_;
  rclcpp::Publisher<ms_msgs::msg::MotionStreamerState>::SharedPtr state_pub_;
  rclcpp::Publisher<ms_msgs::msg::StreamStatistics>::SharedPtr statistics_pub_;
  rclcpp::Publisher<ms_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Service servers
  rclcpp::Service<ms_msgs::srv::GetHealthStatus>::SharedPtr get_health_status_srv_;
  rclcpp::Service<ms_msgs::srv::SetStreamParameters>::SharedPtr set_stream_parameters_srv_;
  rclcpp::Service<ms_msgs::srv::StopStream>::SharedPtr stop_stream_srv_;

  // Timers
  rclcpp::TimerBase::SharedPtr target_timer_;
  rclcpp::TimerBase::SharedPtr statistics_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // State
  State current_state_{State::IDLE};
  State prev_state_{State::IDLE};
  builtin_interfaces::msg::Time state_changed_at_;
  rclcpp::Time uptime_start_;

  // Buffer
  std::mutex buffer_mutex_;
  std::map<std::string, std::queue<CommandEntry>> source_buffers_;

  // Parameters
  double output_frequency_hz_{100.0};
  double max_velocity_{10.0};
  double max_acceleration_{50.0};
  double max_jerk_{200.0};
  size_t max_buffer_depth_{100};
  double heartbeat_rate_hz_{1.0};
  double statistics_rate_hz_{1.0};

  // Statistics
  uint64_t commands_received_{0};
  uint64_t commands_dropped_{0};
  uint64_t commands_limited_{0};
};

}  // namespace ms

#endif  // MS__MS_NODE_HPP_
