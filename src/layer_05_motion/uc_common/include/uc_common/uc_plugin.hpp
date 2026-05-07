#ifndef UC_COMMON__UC_PLUGIN_HPP_
#define UC_COMMON__UC_PLUGIN_HPP_

#include <string>
#include <vector>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/wrench.hpp"
#include "geometry_msgs/msg/twist.hpp"

// Forward declarations for internal components
namespace uc_common
{
class IKSolver;
class TrajectoryPlanner;
class ForceController;
class GripperController;
class SelfCollisionChecker;
class BaseCompensator;

/**
 * @brief Upper Body Control plugin for MC
 *
 * Implements the UpperBodyController interface defined by MC.
 * Shared across bipedal and wheeled morphologies.
 */
class UCPlugin
{
public:
  UCPlugin();
  ~UCPlugin();

  /**
   * @brief Initialize the UC plugin
   * @param robot_type "bipedal" or "wheeled"
   * @param joint_names List of joints controlled by this plugin
   * @param node ROS2 node handle for parameter access
   * @return true on success
   */
  bool init(
    const std::string & robot_type,
    const std::vector<std::string> & joint_names,
    const rclcpp::Node::SharedPtr & node);

  /**
   * @brief Main control cycle update (1kHz)
   *
   * Called by MC in the real-time control thread.
   * Must complete within the allocated time budget.
   *
   * @return true if the control cycle succeeded
   */
  bool update();

  /**
   * @brief Motion mode change notification
   */
  void on_motion_mode_changed(uint8_t new_mode, uint8_t old_mode);

  /**
   * @brief Reset internal state
   */
  void reset();

  /**
   * @brief Emergency stop handler
   *
   * Must complete within 1ms. No blocking operations.
   */
  void emergency_stop();

  std::string get_name() const { return "uc_common"; }
  std::string get_version() const { return "0.1.0"; }

private:
  bool initialized_ = false;
  std::string robot_type_;
  std::vector<std::string> joint_names_;
  rclcpp::Node::SharedPtr node_;

  // Internal components
  std::unique_ptr<IKSolver> ik_solver_;
  std::unique_ptr<TrajectoryPlanner> trajectory_planner_;
  std::unique_ptr<ForceController> force_controller_;
  std::unique_ptr<GripperController> gripper_controller_;
  std::unique_ptr<SelfCollisionChecker> self_collision_checker_;
  std::unique_ptr<BaseCompensator> base_compensator_;

  // State
  uint8_t current_state_ = 0;
  uint16_t error_code_ = 0;
  bool emergency_stopped_ = false;
};

}  // namespace uc_common

#endif  // UC_COMMON__UC_PLUGIN_HPP_
