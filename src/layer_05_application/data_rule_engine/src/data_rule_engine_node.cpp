#include "data_rule_engine/data_rule_engine_node.hpp"

#include <chrono>

namespace data_rule_engine
{

using namespace std::chrono_literals;

DataRuleEngineNode::DataRuleEngineNode(const rclcpp::NodeOptions & options)
: Node("data_rule_engine_node", options)
{
  // Declare parameters
  this->declare_parameter<int>("max_rules", 100);
  this->declare_parameter<int>("evaluation_interval_ms", 100);
  this->declare_parameter<int>("default_cooldown_sec", 30);

  // Publishers
  rule_trigger_event_pub_ = this->create_publisher<data_rule_msgs::msg::RuleTriggerEvent>(
    "/data_rule_engine/rule_trigger_event", 10);
  rule_execution_log_pub_ = this->create_publisher<data_rule_msgs::msg::RuleExecutionLog>(
    "/data_rule_engine/rule_execution_log", 10);
  heartbeat_pub_ = this->create_publisher<data_rule_msgs::msg::Heartbeat>(
    "/data_rule_engine/heartbeat", 10);

  // Services
  deploy_rules_srv_ = this->create_service<data_rule_msgs::srv::DeployRules>(
    "/data_rule_engine/deploy_rules",
    std::bind(&DataRuleEngineNode::handle_deploy_rules, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  get_active_rules_srv_ = this->create_service<data_rule_msgs::srv::GetActiveRules>(
    "/data_rule_engine/get_active_rules",
    std::bind(&DataRuleEngineNode::handle_get_active_rules, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  enable_rule_srv_ = this->create_service<data_rule_msgs::srv::EnableRule>(
    "/data_rule_engine/enable_rule",
    std::bind(&DataRuleEngineNode::handle_enable_rule, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Subscriptions
  diagnosis_sub_ = this->create_subscription<rclcpp::SerializedMessage>(
    "/hds/diagnosis_event", 10,
    std::bind(&DataRuleEngineNode::on_diagnosis_event, this, std::placeholders::_1));

  robot_state_sub_ = this->create_subscription<rclcpp::SerializedMessage>(
    "/sm/robot_state", 10,
    std::bind(&DataRuleEngineNode::on_robot_state, this, std::placeholders::_1));

  // Timers
  heartbeat_timer_ = this->create_wall_timer(
    1s, std::bind(&DataRuleEngineNode::publish_heartbeat, this));
  evaluation_timer_ = this->create_wall_timer(
    100ms, std::bind(&DataRuleEngineNode::evaluate_conditions, this));

  RCLCPP_INFO(this->get_logger(), "DataRuleEngineNode initialized");
}

DataRuleEngineNode::~DataRuleEngineNode()
{
  RCLCPP_INFO(this->get_logger(), "DataRuleEngineNode shutting down");
}

void DataRuleEngineNode::publish_heartbeat()
{
  auto msg = data_rule_msgs::msg::Heartbeat();
  msg.stamp = this->now();
  msg.state = static_cast<uint8_t>(state_.load());
  msg.error_code = error_code_;
  msg.active_rule_count = static_cast<uint32_t>(active_rules_.size());
  msg.triggered_today = 0;
  heartbeat_pub_->publish(msg);
}

void DataRuleEngineNode::evaluate_conditions()
{
  // TODO: Implement condition evaluation logic
}

void DataRuleEngineNode::transition_to(State new_state)
{
  auto old_state = state_.exchange(new_state);
  if (old_state != new_state) {
    RCLCPP_INFO(this->get_logger(), "State transition: %d -> %d",
                static_cast<int>(old_state), static_cast<int>(new_state));
  }
}

void DataRuleEngineNode::handle_deploy_rules(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_rule_msgs::srv::DeployRules::Request> request,
  std::shared_ptr<data_rule_msgs::srv::DeployRules::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "DeployRules request: replace_all=%s",
              request->replace_all ? "true" : "false");
  // TODO: Implement rule deployment logic
  response->success = false;
  response->message = "Not implemented";
}

void DataRuleEngineNode::handle_get_active_rules(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_rule_msgs::srv::GetActiveRules::Request>,
  std::shared_ptr<data_rule_msgs::srv::GetActiveRules::Response> response)
{
  response->success = true;
  response->rules = active_rules_;
}

void DataRuleEngineNode::handle_enable_rule(
  const std::shared_ptr<rmw_request_id_t>,
  const std::shared_ptr<data_rule_msgs::srv::EnableRule::Request> request,
  std::shared_ptr<data_rule_msgs::srv::EnableRule::Response> response)
{
  RCLCPP_INFO(this->get_logger(), "EnableRule request: rule_id=%s enabled=%s",
              request->rule_id.c_str(), request->enabled ? "true" : "false");
  // TODO: Implement enable/disable logic
  response->success = false;
  response->message = "Not implemented";
}

void DataRuleEngineNode::on_diagnosis_event(const std::shared_ptr<rclcpp::SerializedMessage>)
{
  // TODO: Evaluate diagnosis-related rules
}

void DataRuleEngineNode::on_robot_state(const std::shared_ptr<rclcpp::SerializedMessage>)
{
  // TODO: Evaluate state-related rules
}

}  // namespace data_rule_engine

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(data_rule_engine::DataRuleEngineNode)
