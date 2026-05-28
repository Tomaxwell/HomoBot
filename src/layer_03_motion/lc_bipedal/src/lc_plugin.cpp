#include "lc_bipedal/lc_plugin.hpp"

#include "lc_bipedal/rl_policy.hpp"
#include "lc_bipedal/wbc_controller.hpp"
#include "lc_bipedal/contact_estimator.hpp"
#include "lc_bipedal/gait_scheduler.hpp"
#include "lc_bipedal/reference_generator.hpp"
#include "lc_bipedal/state_estimator.hpp"
#include "lc_bipedal/balance_monitor.hpp"

#include "rclcpp/rclcpp.hpp"

namespace lc_bipedal
{

LCBipedalPlugin::LCBipedalPlugin()
: rl_policy_(std::make_unique<RLPolicy>()),
  wbc_controller_(std::make_unique<WBCController>()),
  contact_estimator_(std::make_unique<ContactEstimator>()),
  gait_scheduler_(std::make_unique<GaitScheduler>()),
  reference_generator_(std::make_unique<ReferenceGenerator>()),
  state_estimator_(std::make_unique<StateEstimator>()),
  balance_monitor_(std::make_unique<BalanceMonitor>())
{
}

LCBipedalPlugin::~LCBipedalPlugin() = default;

bool LCBipedalPlugin::init(
  const std::string & robot_type,
  const std::vector<std::string> & joint_names,
  const rclcpp::Node::SharedPtr & node)
{
  if (initialized_) {
    RCLCPP_WARN(node->get_logger(), "LCBipedalPlugin already initialized");
    return false;
  }

  robot_type_ = robot_type;
  joint_names_ = joint_names;
  node_ = node;

  // Load parameters
  std::string policy_model = node->get_parameter_or(
    "lc_bipedal.policy_model", std::string("model/policy.onnx"));
  double gait_period = node->get_parameter_or("lc_bipedal.gait_period", 0.8);
  double duty_factor = node->get_parameter_or("lc_bipedal.duty_factor", 0.6);
  double com_height = node->get_parameter_or("lc_bipedal.com_height", 0.75);
  double step_height = node->get_parameter_or("lc_bipedal.step_height", 0.08);
  double balance_com_threshold = node->get_parameter_or(
    "lc_bipedal.balance_com_threshold", 0.02);
  double balance_angular_threshold = node->get_parameter_or(
    "lc_bipedal.balance_angular_threshold", 0.3);

  // Initialize components
  if (!rl_policy_>init(policy_model)) {
    RCLCPP_ERROR(node->get_logger(), "RLPolicy init failed");
    return false;
  }
  if (!wbc_controller_>init(joint_names.size(), 2)) {
    RCLCPP_ERROR(node->get_logger(), "WBCController init failed");
    return false;
  }
  if (!contact_estimator_>init(joint_names.size())) {
    RCLCPP_ERROR(node->get_logger(), "ContactEstimator init failed");
    return false;
  }
  if (!gait_scheduler_>init(gait_period, duty_factor)) {
    RCLCPP_ERROR(node->get_logger(), "GaitScheduler init failed");
    return false;
  }
  if (!reference_generator_>init(com_height, step_height)) {
    RCLCPP_ERROR(node->get_logger(), "ReferenceGenerator init failed");
    return false;
  }
  if (!state_estimator_>init(joint_names.size())) {
    RCLCPP_ERROR(node->get_logger(), "StateEstimator init failed");
    return false;
  }
  if (!balance_monitor_>init(balance_com_threshold, balance_angular_threshold)) {
    RCLCPP_ERROR(node->get_logger(), "BalanceMonitor init failed");
    return false;
  }

  cached_safe_action_.resize(joint_names.size(), 0.0);
  initialized_ = true;
  RCLCPP_INFO(node->get_logger(), "LCBipedalPlugin initialized (%s, %zu joints)",
    robot_type_.c_str(), joint_names_.size());
  return true;
}

bool LCBipedalPlugin::update()
{
  if (!initialized_ || emergency_stopped_) {
    return false;
  }

  // TODO: Implement 1kHz bipedal control cycle
  // 1. Update state estimator (foot FK, support polygon)
  // 2. Estimate contact forces (inverse dynamics)
  // 3. Update gait scheduler FSM
  // 4. Generate reference trajectories
  // 5. Build observation vector for RL policy
  // 6. Run RL policy inference (or read cached async result)
  // 7. Solve WBC QP
  // 8. Check balance monitor
  // 9. Output joint torques

  return true;
}

void LCBipedalPlugin::on_motion_mode_changed(uint8_t new_mode, uint8_t old_mode)
{
  (void)old_mode;
  RCLCPP_INFO(node_>get_logger(), "LC bipedal motion mode: %u -> %u", old_mode, new_mode);
  gait_scheduler_>reset();
  reference_generator_>reset();
}

void LCBipedalPlugin::reset()
{
  rl_policy_>reset();
  wbc_controller_>reset();
  contact_estimator_>reset();
  gait_scheduler_>reset();
  reference_generator_>reset();
  state_estimator_>reset();
  balance_monitor_>reset();
  consecutive_nan_count_ = 0;
  emergency_stopped_ = false;
}

void LCBipedalPlugin::emergency_stop()
{
  emergency_stopped_ = true;
  rl_policy_>stop_inference_thread();
  std::fill(cached_safe_action_.begin(), cached_safe_action_.end(), 0.0);
  consecutive_nan_count_ = 0;
  gait_scheduler_>reset();
}

}  // namespace lc_bipedal
