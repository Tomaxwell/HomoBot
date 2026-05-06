#include "uc_common/uc_plugin.hpp"

#include "uc_common/ik_solver.hpp"
#include "uc_common/trajectory_planner.hpp"
#include "uc_common/force_controller.hpp"
#include "uc_common/gripper_controller.hpp"
#include "uc_common/self_collision_checker.hpp"
#include "uc_common/base_compensator.hpp"

#include "rclcpp/rclcpp.hpp"

namespace uc_common
{

UCPlugin::UCPlugin()
: ik_solver_(std::make_unique<IKSolver>()),
  trajectory_planner_(std::make_unique<TrajectoryPlanner>()),
  force_controller_(std::make_unique<ForceController>()),
  gripper_controller_(std::make_unique<GripperController>()),
  self_collision_checker_(std::make_unique<SelfCollisionChecker>()),
  base_compensator_(std::make_unique<BaseCompensator>())
{
}

UCPlugin::~UCPlugin() = default;

bool UCPlugin::init(
  const std::string & robot_type,
  const std::vector<std::string> & joint_names,
  const rclcpp::Node::SharedPtr & node)
{
  if (initialized_) {
    RCLCPP_WARN(node->get_logger(), "UCPlugin already initialized");
    return false;
  }

  robot_type_ = robot_type;
  joint_names_ = joint_names;
  node_ = node;

  // Load parameters
  double max_velocity = node->get_parameter_or("uc.max_velocity", 0.5);
  double max_acceleration = node->get_parameter_or("uc.max_acceleration", 1.0);
  double force_mass = node->get_parameter_or("uc.force_mass", 1.0);
  double force_damping = node->get_parameter_or("uc.force_damping", 10.0);
  double force_stiffness = node->get_parameter_or("uc.force_stiffness", 100.0);
  double gripper_max_width = node->get_parameter_or("uc.gripper_max_width", 0.08);
  double gripper_max_effort = node->get_parameter_or("uc.gripper_max_effort", 20.0);
  double compensator_bandwidth = node->get_parameter_or("uc.compensator_bandwidth", 10.0);

  // Initialize components
  if (!ik_solver_>init(joint_names)) {
    RCLCPP_ERROR(node->get_logger(), "IKSolver init failed");
    return false;
  }
  if (!trajectory_planner_>init(max_velocity, max_acceleration)) {
    RCLCPP_ERROR(node->get_logger(), "TrajectoryPlanner init failed");
    return false;
  }
  if (!force_controller_>init(force_mass, force_damping, force_stiffness)) {
    RCLCPP_ERROR(node->get_logger(), "ForceController init failed");
    return false;
  }
  if (!gripper_controller_>init(gripper_max_width, gripper_max_effort)) {
    RCLCPP_ERROR(node->get_logger(), "GripperController init failed");
    return false;
  }
  if (!self_collision_checker_>init(joint_names)) {
    RCLCPP_ERROR(node->get_logger(), "SelfCollisionChecker init failed");
    return false;
  }
  if (!base_compensator_>init(compensator_bandwidth)) {
    RCLCPP_ERROR(node->get_logger(), "BaseCompensator init failed");
    return false;
  }

  initialized_ = true;
  RCLCPP_INFO(node->get_logger(), "UCPlugin initialized (%s, %zu joints)",
    robot_type_.c_str(), joint_names_.size());
  return true;
}

bool UCPlugin::update()
{
  if (!initialized_ || emergency_stopped_) {
    return false;
  }

  // TODO: Implement 1kHz control cycle
  // 1. Read end-effector commands from cache
  // 2. Run IK solver
  // 3. Plan trajectories
  // 4. Apply force control if needed
  // 5. Update gripper states
  // 6. Check self-collision
  // 7. Apply base compensation
  // 8. Output joint commands

  return true;
}

void UCPlugin::on_motion_mode_changed(uint8_t new_mode, uint8_t old_mode)
{
  (void)old_mode;
  RCLCPP_INFO(node_>get_logger(), "UC motion mode changed: %u -> %u", old_mode, new_mode);
  // Reset internal trajectory state on mode change
  trajectory_planner_>reset();
  force_controller_>reset();
}

void UCPlugin::reset()
{
  ik_solver_>reset();
  trajectory_planner_>reset();
  force_controller_>reset();
  gripper_controller_>reset();
  self_collision_checker_>reset();
  base_compensator_>reset();
  error_code_ = 0;
  current_state_ = 0;
  emergency_stopped_ = false;
}

void UCPlugin::emergency_stop()
{
  emergency_stopped_ = true;
  // Clear trajectory targets
  trajectory_planner_>reset();
  force_controller_>reset();
  // Do not deallocate or block
}

}  // namespace uc_common
