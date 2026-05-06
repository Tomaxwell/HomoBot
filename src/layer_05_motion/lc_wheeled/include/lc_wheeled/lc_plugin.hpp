#ifndef LC_WHEELED__LC_PLUGIN_HPP_
#define LC_WHEELED__LC_PLUGIN_HPP_

#include <string>
#include <vector>
#include <memory>

#include "rclcpp/rclcpp.hpp"

// Forward declarations
namespace lc_wheeled
{
class ChassisController;
class LiftController;
class CasterMonitor;

/**
 * @brief Lower Body Control plugin for wheeled morphology
 *
 * Controls chassis movement (differential drive) and lift column.
 */
class LCWheeledPlugin
{
public:
  LCWheeledPlugin();
  ~LCWheeledPlugin();

  bool init(
    const std::string & robot_type,
    const std::vector<std::string> & joint_names,
    const rclcpp::Node::SharedPtr & node);

  /**
   * @brief Main control cycle update (1kHz)
   */
  bool update();

  void on_motion_mode_changed(uint8_t new_mode, uint8_t old_mode);
  void reset();
  void emergency_stop();

  std::string get_name() const { return "lc_wheeled"; }
  std::string get_version() const { return "0.1.0"; }

private:
  bool initialized_ = false;
  bool emergency_stopped_ = false;
  std::string robot_type_;
  std::vector<std::string> joint_names_;
  rclcpp::Node::SharedPtr node_;

  std::unique_ptr<ChassisController> chassis_controller_;
  std::unique_ptr<LiftController> lift_controller_;
  std::unique_ptr<CasterMonitor> caster_monitor_;
};

}  // namespace lc_wheeled

#endif  // LC_WHEELED__LC_PLUGIN_HPP_
