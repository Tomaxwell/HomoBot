#ifndef RC__RC_NODE_HPP_
#define RC__RC_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <map>
#include <vector>
#include <deque>

#include "rc_msgs/msg/system_metrics.hpp"
#include "rc_msgs/msg/module_health.hpp"
#include "rc_msgs/msg/system_event.hpp"
#include "rc_msgs/msg/log_entry.hpp"
#include "rc_msgs/msg/heartbeat.hpp"
#include "rc_msgs/srv/get_health_status.hpp"
#include "rc_msgs/srv/query_events.hpp"
#include "rc_msgs/srv/query_logs.hpp"
#include "rc_msgs/srv/query_metrics.hpp"
#include "rc_msgs/srv/get_system_summary.hpp"

namespace rc
{

class ResourceCollectionNode : public rclcpp::Node
{
public:
  explicit ResourceCollectionNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~ResourceCollectionNode() override;

private:
  enum class State : uint8_t
  {
    IDLE = 0,
    ACTIVE = 1,
    FAULT = 2
  };

  void publish_metrics();
  void publish_module_health();
  void publish_heartbeat();
  void transition_to(State new_state);

  // Service handlers
  void handle_get_health_status(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<rc_msgs::srv::GetHealthStatus::Request> request,
    std::shared_ptr<rc_msgs::srv::GetHealthStatus::Response> response);

  void handle_query_events(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<rc_msgs::srv::QueryEvents::Request> request,
    std::shared_ptr<rc_msgs::srv::QueryEvents::Response> response);

  void handle_query_logs(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<rc_msgs::srv::QueryLogs::Request> request,
    std::shared_ptr<rc_msgs::srv::QueryLogs::Response> response);

  void handle_query_metrics(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<rc_msgs::srv::QueryMetrics::Request> request,
    std::shared_ptr<rc_msgs::srv::QueryMetrics::Response> response);

  void handle_get_system_summary(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<rc_msgs::srv::GetSystemSummary::Request> request,
    std::shared_ptr<rc_msgs::srv::GetSystemSummary::Response> response);

  // Heartbeat subscription (to track module health)
  void on_heartbeat(const rc_msgs::msg::Heartbeat::SharedPtr msg);

  // Publishers
  rclcpp::Publisher<rc_msgs::msg::SystemMetrics>::SharedPtr metrics_pub_;
  rclcpp::Publisher<rc_msgs::msg::ModuleHealth>::SharedPtr module_health_pub_;
  rclcpp::Publisher<rc_msgs::msg::SystemEvent>::SharedPtr system_event_pub_;
  rclcpp::Publisher<rc_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Subscribers
  rclcpp::Subscription<rc_msgs::msg::Heartbeat>::SharedPtr heartbeat_sub_;

  // Service servers
  rclcpp::Service<rc_msgs::srv::GetHealthStatus>::SharedPtr get_health_status_srv_;
  rclcpp::Service<rc_msgs::srv::QueryEvents>::SharedPtr query_events_srv_;
  rclcpp::Service<rc_msgs::srv::QueryLogs>::SharedPtr query_logs_srv_;
  rclcpp::Service<rc_msgs::srv::QueryMetrics>::SharedPtr query_metrics_srv_;
  rclcpp::Service<rc_msgs::srv::GetSystemSummary>::SharedPtr get_system_summary_srv_;

  // Timers
  rclcpp::TimerBase::SharedPtr metrics_timer_;
  rclcpp::TimerBase::SharedPtr module_health_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // State
  State current_state_{State::IDLE};
  rclcpp::Time uptime_start_;

  // Module tracking
  std::map<std::string, rc_msgs::msg::ModuleHealth> module_health_map_;
  std::mutex module_mutex_;

  // History
  std::deque<rc_msgs::msg::SystemMetrics> metrics_history_;
  std::deque<rc_msgs::msg::SystemEvent> event_history_;
  std::deque<rc_msgs::msg::LogEntry> log_history_;
  std::mutex history_mutex_;

  // Parameters
  double metrics_rate_hz_{1.0};
  double heartbeat_rate_hz_{1.0};
  size_t max_history_size_{10000};
};

}  // namespace rc

#endif  // RC__RC_NODE_HPP_
