#ifndef LC_BIPEDAL__LC_PLUGIN_HPP_
#define LC_BIPEDAL__LC_PLUGIN_HPP_

#include <string>
#include <vector>
#include <memory>

#include "rclcpp/rclcpp.hpp"

// Forward declarations for internal components
namespace lc_bipedal
{
class RLPolicy;
class WBCController;
class ContactEstimator;
class GaitScheduler;
class ReferenceGenerator;
class StateEstimator;
class BalanceMonitor;

/**
 * @brief Lower Body Control plugin for bipedal morphology
 *
 * Implements RL-based policy + QP-based WBC + gait FSM.
 */
class LCBipedalPlugin
{
public:
  LCBipedalPlugin();
  ~LCBipedalPlugin();

  /**
   * @brief Initialize the LC plugin
   */
  bool init(
    const std::string & robot_type,
    const std::vector<std::string> & joint_names,
    const rclcpp::Node::SharedPtr & node);

  /**
   * @brief Main control cycle update (1kHz)
   * @return true if control cycle succeeded
   */
  bool update();

  void on_motion_mode_changed(uint8_t new_mode, uint8_t old_mode);
  void reset();
  void emergency_stop();

  std::string get_name() const { return "lc_bipedal"; }
  std::string get_version() const { return "0.1.0"; }

private:
  bool initialized_ = false;
  bool emergency_stopped_ = false;
  std::string robot_type_;
  std::vector<std::string> joint_names_;
  rclcpp::Node::SharedPtr node_;

  // Internal components
  std::unique_ptr<RLPolicy> rl_policy_;
  std::unique_ptr<WBCController> wbc_controller_;
  std::unique_ptr<ContactEstimator> contact_estimator_;
  std::unique_ptr<GaitScheduler> gait_scheduler_;
  std::unique_ptr<ReferenceGenerator> reference_generator_;
  std::unique_ptr<StateEstimator> state_estimator_;
  std::unique_ptr<BalanceMonitor> balance_monitor_;

  // Cached safe action for emergency stop
  std::vector<double> cached_safe_action_;
  uint8_t consecutive_nan_count_ = 0;
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__LC_PLUGIN_HPP_
