#ifndef LC_WHEELED__LIFT_CONTROLLER_HPP_
#define LC_WHEELED__LIFT_CONTROLLER_HPP_

namespace lc_wheeled
{

/**
 * @brief Lift column PID position controller
 */
class LiftController
{
public:
  LiftController();
  ~LiftController();

  bool init(
    double min_height,
    double max_height,
    double max_velocity,
    double kp,
    double ki,
    double kd);

  /**
   * @brief Update lift control
   * @param target_height Target height [m]
   * @param current_height Current height [m]
   * @param dt Time step [s]
   * @param out_velocity Output velocity command [m/s]
   * @return true if target is reachable
   */
  bool update(
    double target_height,
    double current_height,
    double dt,
    double & out_velocity);

  bool is_in_position() const { return in_position_; }

  void reset();

private:
  bool initialized_ = false;
  double min_height_ = 0.0;
  double max_height_ = 1.0;
  double max_velocity_ = 0.1;  // m/s
  double kp_ = 10.0;
  double ki_ = 0.0;
  double kd_ = 1.0;
  double integral_ = 0.0;
  double prev_error_ = 0.0;
  bool in_position_ = true;
};

}  // namespace lc_wheeled

#endif  // LC_WHEELED__LIFT_CONTROLLER_HPP_
