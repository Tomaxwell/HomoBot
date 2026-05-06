#include "rc/rc_node.hpp"

#include <chrono>
#include <fstream>

namespace rc
{

ResourceCollectionNode::ResourceCollectionNode(const rclcpp::NodeOptions &options)
: Node("rc_node", options)
{
  this->declare_parameter("metrics_rate_hz", 1.0);
  this->declare_parameter("heartbeat_rate_hz", 1.0);
  this->declare_parameter("max_history_size", 10000);

  metrics_rate_hz_ = this->get_parameter("metrics_rate_hz").as_double();
  heartbeat_rate_hz_ = this->get_parameter("heartbeat_rate_hz").as_double();
  max_history_size_ = this->get_parameter("max_history_size").as_int();

  uptime_start_ = this->now();

  // Subscribers (subscribe to all module heartbeats)
  heartbeat_sub_ = this->create_subscription<rc_msgs::msg::Heartbeat>(
    "/+/heartbeat", rclcpp::QoS(10),
    std::bind(&ResourceCollectionNode::on_heartbeat, this, std::placeholders::_1));

  // Publishers
  metrics_pub_ = this->create_publisher<rc_msgs::msg::SystemMetrics>(
    "/rc/system_metrics", rclcpp::QoS(1));

  module_health_pub_ = this->create_publisher<rc_msgs::msg::ModuleHealth>(
    "/rc/module_health", rclcpp::QoS(1));

  system_event_pub_ = this->create_publisher<rc_msgs::msg::SystemEvent>(
    "/rc/system_event", rclcpp::QoS(100));

  heartbeat_pub_ = this->create_publisher<rc_msgs::msg::Heartbeat>(
    "/rc/heartbeat", rclcpp::QoS(1));

  // Service servers
  get_health_status_srv_ = this->create_service<rc_msgs::srv::GetHealthStatus>(
    "/rc/get_health_status",
    std::bind(&ResourceCollectionNode::handle_get_health_status, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  query_events_srv_ = this->create_service<rc_msgs::srv::QueryEvents>(
    "/rc/query_events",
    std::bind(&ResourceCollectionNode::handle_query_events, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  query_logs_srv_ = this->create_service<rc_msgs::srv::QueryLogs>(
    "/rc/query_logs",
    std::bind(&ResourceCollectionNode::handle_query_logs, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  query_metrics_srv_ = this->create_service<rc_msgs::srv::QueryMetrics>(
    "/rc/query_metrics",
    std::bind(&ResourceCollectionNode::handle_query_metrics, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_system_summary_srv_ = this->create_service<rc_msgs::srv::GetSystemSummary>(
    "/rc/get_system_summary",
    std::bind(&ResourceCollectionNode::handle_get_system_summary, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Timers
  metrics_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / metrics_rate_hz_),
    std::bind(&ResourceCollectionNode::publish_metrics, this));

  module_health_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / metrics_rate_hz_),
    std::bind(&ResourceCollectionNode::publish_module_health, this));

  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(1.0 / heartbeat_rate_hz_),
    std::bind(&ResourceCollectionNode::publish_heartbeat, this));

  RCLCPP_INFO(this->get_logger(), "ResourceCollection node initialized");
}

ResourceCollectionNode::~ResourceCollectionNode()
{
  RCLCPP_INFO(this->get_logger(), "ResourceCollection node shutting down");
}

void ResourceCollectionNode::publish_metrics()
{
  auto msg = rc_msgs::msg::SystemMetrics();
  msg.stamp = this->now();

  // TODO: Read actual system metrics from /proc, sysfs, etc.
  msg.cpu_percent = 0.0f;
  msg.memory_percent = 0.0f;
  msg.disk_percent = 0.0f;
  msg.temperature_celsius = 0.0f;

  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    metrics_history_.push_back(msg);
    if (metrics_history_.size() > max_history_size_) {
      metrics_history_.pop_front();
    }
  }

  metrics_pub_->publish(msg);
}

void ResourceCollectionNode::publish_module_health()
{
  std::lock_guard<std::mutex> lock(module_mutex_);
  for (const auto &[name, health] : module_health_map_) {
    module_health_pub_->publish(health);
  }
}

void ResourceCollectionNode::publish_heartbeat()
{
  auto msg = rc_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.node_name = this->get_name();

  {
    std::lock_guard<std::mutex> lock(module_mutex_);
    msg.modules_online = 0;
    for (const auto &[name, health] : module_health_map_) {
      if (health.status == rc_msgs::msg::ModuleHealth::STATUS_ONLINE) {
        msg.modules_online++;
      }
    }
    msg.total_modules = module_health_map_.size();
  }

  msg.collection_rate_hz = metrics_rate_hz_;
  heartbeat_pub_->publish(msg);
}

void ResourceCollectionNode::transition_to(State new_state)
{
  if (current_state_ != new_state) {
    current_state_ = new_state;
  }
}

void ResourceCollectionNode::on_heartbeat(const rc_msgs::msg::Heartbeat::SharedPtr msg)
{
  std::lock_guard<std::mutex> lock(module_mutex_);

  auto &health = module_health_map_[msg->node_name];
  health.module_name = msg->node_name;
  health.status = rc_msgs::msg::ModuleHealth::STATUS_ONLINE;
  health.last_heartbeat = msg->stamp;
  health.heartbeat_freq_hz = heartbeat_rate_hz_;
}

void ResourceCollectionNode::handle_get_health_status(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<rc_msgs::srv::GetHealthStatus::Request>,
  std::shared_ptr<rc_msgs::srv::GetHealthStatus::Response> response)
{
  response->healthy = current_state_ != State::FAULT;
  response->node_name = this->get_name();

  {
    std::lock_guard<std::mutex> lock(module_mutex_);
    uint32_t online = 0;
    for (const auto &[name, health] : module_health_map_) {
      if (health.status == rc_msgs::msg::ModuleHealth::STATUS_ONLINE) {
        online++;
      }
    }
    response->modules_online = online;
  }
}

void ResourceCollectionNode::handle_query_events(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<rc_msgs::srv::QueryEvents::Request> request,
  std::shared_ptr<rc_msgs::srv::QueryEvents::Response> response)
{
  std::lock_guard<std::mutex> lock(history_mutex_);
  response->events.clear();

  for (const auto &event : event_history_) {
    auto t = rclcpp::Time(event.timestamp);
    if (t >= rclcpp::Time(request->start_time) && t <= rclcpp::Time(request->end_time)) {
      if (request->category_filter.empty() || event.category == request->category_filter) {
        if (event.severity >= request->min_severity) {
          response->events.push_back(event);
        }
      }
    }
  }

  response->success = true;
  response->count = response->events.size();
}

void ResourceCollectionNode::handle_query_logs(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<rc_msgs::srv::QueryLogs::Request> request,
  std::shared_ptr<rc_msgs::srv::QueryLogs::Response> response)
{
  std::lock_guard<std::mutex> lock(history_mutex_);
  response->logs.clear();

  for (const auto &log : log_history_) {
    auto t = rclcpp::Time(log.timestamp);
    if (t >= rclcpp::Time(request->start_time) && t <= rclcpp::Time(request->end_time)) {
      if (request->module_filter.empty() || log.module_name == request->module_filter) {
        if (log.level >= request->min_level) {
          response->logs.push_back(log);
        }
      }
    }
  }

  response->success = true;
  response->count = response->logs.size();
}

void ResourceCollectionNode::handle_query_metrics(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<rc_msgs::srv::QueryMetrics::Request> request,
  std::shared_ptr<rc_msgs::srv::QueryMetrics::Response> response)
{
  std::lock_guard<std::mutex> lock(history_mutex_);
  response->metrics.clear();

  for (const auto &metric : metrics_history_) {
    auto t = rclcpp::Time(metric.stamp);
    if (t >= rclcpp::Time(request->start_time) && t <= rclcpp::Time(request->end_time)) {
      response->metrics.push_back(metric);
    }
  }

  response->success = true;
  response->count = response->metrics.size();
}

void ResourceCollectionNode::handle_get_system_summary(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<rc_msgs::srv::GetSystemSummary::Request>,
  std::shared_ptr<rc_msgs::srv::GetSystemSummary::Response> response)
{
  response->success = true;

  // Current metrics
  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    if (!metrics_history_.empty()) {
      response->current_metrics = metrics_history_.back();
    }
  }

  // Module healths
  {
    std::lock_guard<std::mutex> lock(module_mutex_);
    response->module_healths.clear();
    for (const auto &[name, health] : module_health_map_) {
      response->module_healths.push_back(health);
    }
  }

  // 24h stats
  response->total_events_24h = 0;
  response->total_errors_24h = 0;
  response->avg_cpu_24h = 0.0f;
  response->avg_memory_24h = 0.0f;
}

}  // namespace rc

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(rc::ResourceCollectionNode)
