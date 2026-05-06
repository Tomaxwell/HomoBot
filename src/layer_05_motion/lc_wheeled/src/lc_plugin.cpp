#include "lc_wheeled/lc_plugin.hpp"

#include "lc_wheeled/chassis_controller.hpp"
#include "lc_wheeled/lift_controller.hpp"
#include "lc_wheeled/caster_monitor.hpp"

#include "rclcpp/rclcpp.hpp"

namespace lc_wheeled
{

LCWheeledPlugin::LCWheeledPlugin()
: chassis_controller_(std::make_unique<ChassisController>()),
  lift_controller_(std::make_unique<LiftController>()),
  caster_monitor_(std::make_unique<CasterMonitor>())
{
}

LCWheeledPlugin::~LCWheeledPlugin() = default;

bool LCWheeledPlugin::init(
  const std::string & robot_type,
  const std::vector<std::string> & joint_names,
  const rclcpp::Node::SharedPtr & node)
{
  if (initialized_) {
    RCLCPP_WARN(node->get_logger(), "LCWheeledPlugin already initialized");
    return false;
  }

  robot_type_ = robot_type;
  joint_names_ = joint_names;
  node_ = node;

  // Load parameters
  double wheel_radius = node->get_parameter_or("lc_wheeled.wheel_radius", 0.05);
  double wheel_base = node->get_parameter_or("lc_wheeled.wheel_base", 0.4);
  double max_linear_speed = node->get_parameter_or("lc_wheeled.max_linear_speed", 1.0);
  double max_angular_speed = node->get_parameter_or("lc_wheeled.max_angular_speed", 1.0);
  double lift_min = node->get_parameter_or("lc_wheeled.lift_min_height", 0.0);
  double lift_max = node->get_parameter_or("lc_wheeled.lift_max_height", 1.0);
  double lift_max_vel = node->get_parameter_or("lc_wheeled.lift_max_velocity", 0.1);
  double lift_kp = node->get_parameter_or("lc_wheeled.lift_kp", 10.0);
  double lift_ki = node->get_parameter_or("lc_wheeled.lift_ki", 0.0);
  double lift_kd = node->get_parameter_or("lc_wheeled.lift_kd", 1.0);
  size_t num_casters = node->get_parameter_or("lc_wheeled.num_casters", 2);

  if (!chassis_controller_>init(wheel_radius, wheel_base, max_linear_speed, max_angular_speed)) {
    RCLCPP_ERROR(node->get_logger(), "ChassisController init failed");
    return false;
  }
  if (!lift_controller_>init(lift_min, lift_max, lift_max_vel, lift_kp, lift_ki, lift_kd)) {
    RCLCPP_ERROR(node->get_logger(), "LiftController init failed");
    return false;
  }
  if (!caster_monitor_>init(num_casters)) {
    RCLCPP_ERROR(node->get_logger(), "CasterMonitor init failed");
    return false;
  }

  initialized_ = true;
  RCLCPP_INFO(node->get_logger(), "LCWheeledPlugin initialized (%s, %zu joints)",
    robot_type_.c_str(), joint_names_.size());
  return true;
}

bool LCWheeledPlugin::update()
{
  if (!initialized_ || emergency_stopped_) {
    return false;
  }

  // TODO: Implement 1kHz wheeled control cycle
  // 1. Read LocomotionCommand (vx, yaw_rate)
  // 2. Compute wheel velocities via chassis controller
  // 3. Update lift column position control
  // 4. Monitor caster states
  // 5. Output joint commands

  return true;
}

void LCWheeledPlugin::on_motion_mode_changed(uint8_t new_mode, uint8_t old_mode)
{
  (void)old_mode;
  RCLCPP_INFO(node_>get_logger(), "LC wheeled motion mode: %u -> %u", old_mode, new_mode);
  chassis_controller_>reset();
  lift_controller_>reset();
}

void LCWheeledPlugin::reset()
{
  chassis_controller_>reset();
  lift_controller_>reset();
  caster_monitor_>reset();
  emergency_stopped_ = false;
}

void LCWheeledPlugin::emergency_stop()
{
  emergency_stopped_ = true;
  chassis_controller_>reset();
  lift_controller_>reset();
}

}  // namespace lc_wheeled
