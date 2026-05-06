#ifndef LC_WHEELED__CHASSIS_CONTROLLER_HPP_
#define LC_WHEELED__CHASSIS_CONTROLLER_HPP_

namespace lc_wheeled
{

/**
 * @brief Differential drive chassis controller
 *
 * Converts Twist commands to wheel velocities using
 * differential drive kinematics + PID control.
 */
class ChassisController
{
public:
  ChassisController();
  ~ChassisController();

  bool init(
    double wheel_radius,
    double wheel_base,
    double max_linear_speed,
    double max_angular_speed);

  /**
   * @brief Compute wheel velocities from twist command
   * @param vx Forward velocity [m/s]
   * @param yaw_rate Angular velocity [rad/s]
   * @param out_left_wheel_vel Left wheel velocity [rad/s]
   * @param out_right_wheel_vel Right wheel velocity [rad/s]
   */
  void update(
    double vx,
    double yaw_rate,
    double & out_left_wheel_vel,
    double & out_right_wheel_vel);

  void reset();

private:
  bool initialized_ = false;
  double wheel_radius_ = 0.05;     // m
  double wheel_base_ = 0.4;        // m
  double max_linear_speed_ = 1.0;  // m/s
  double max_angular_speed_ = 1.0; // rad/s
};

}  // namespace lc_wheeled

#endif  // LC_WHEELED__CHASSIS_CONTROLLER_HPP_
