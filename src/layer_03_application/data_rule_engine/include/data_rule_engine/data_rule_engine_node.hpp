#ifndef DATA_RULE_ENGINE__DATA_RULE_ENGINE_NODE_HPP_
#define DATA_RULE_ENGINE__DATA_RULE_ENGINE_NODE_HPP_

#include <rclcpp/rclcpp.hpp>

#include "data_rule_msgs/msg/rule_definition.hpp"
#include "data_rule_msgs/msg/rule_trigger_event.hpp"
#include "data_rule_msgs/msg/rule_execution_log.hpp"
#include "data_rule_msgs/msg/heartbeat.hpp"
#include "data_rule_msgs/srv/deploy_rules.hpp"
#include "data_rule_msgs/srv/get_active_rules.hpp"
#include "data_rule_msgs/srv/enable_rule.hpp"

namespace data_rule_engine
{

enum class State : uint8_t
{
  STANDBY = 0,
  ACTIVE = 1,
  RULE_UPDATING = 2,
  FAULT = 3
};

class DataRuleEngineNode : public rclcpp::Node
{
public:
  explicit DataRuleEngineNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~DataRuleEngineNode() override;

private:
  void publish_heartbeat();
  void evaluate_conditions();
  void transition_to(State new_state);

  // Service handlers
  void handle_deploy_rules(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_rule_msgs::srv::DeployRules::Request> request,
    std::shared_ptr<data_rule_msgs::srv::DeployRules::Response> response);

  void handle_get_active_rules(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_rule_msgs::srv::GetActiveRules::Request> request,
    std::shared_ptr<data_rule_msgs::srv::GetActiveRules::Response> response);

  void handle_enable_rule(
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<data_rule_msgs::srv::EnableRule::Request> request,
    std::shared_ptr<data_rule_msgs::srv::EnableRule::Response> response);

  // Topic callbacks
  void on_diagnosis_event(const std::shared_ptr<rclcpp::SerializedMessage> msg);
  void on_robot_state(const std::shared_ptr<rclcpp::SerializedMessage> msg);

  // Publishers
  rclcpp::Publisher<data_rule_msgs::msg::RuleTriggerEvent>::SharedPtr rule_trigger_event_pub_;
  rclcpp::Publisher<data_rule_msgs::msg::RuleExecutionLog>::SharedPtr rule_execution_log_pub_;
  rclcpp::Publisher<data_rule_msgs::msg::Heartbeat>::SharedPtr heartbeat_pub_;

  // Services
  rclcpp::Service<data_rule_msgs::srv::DeployRules>::SharedPtr deploy_rules_srv_;
  rclcpp::Service<data_rule_msgs::srv::GetActiveRules>::SharedPtr get_active_rules_srv_;
  rclcpp::Service<data_rule_msgs::srv::EnableRule>::SharedPtr enable_rule_srv_;

  // Subscriptions (generic for dynamic topic support)
  rclcpp::SubscriptionBase::SharedPtr diagnosis_sub_;
  rclcpp::SubscriptionBase::SharedPtr robot_state_sub_;

  // Timers
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::TimerBase::SharedPtr evaluation_timer_;

  // State
  std::atomic<State> state_{State::STANDBY};
  uint32_t error_code_{0};
  std::vector<data_rule_msgs::msg::RuleDefinition> active_rules_;
};

}  // namespace data_rule_engine

#endif  // DATA_RULE_ENGINE__DATA_RULE_ENGINE_NODE_HPP_
