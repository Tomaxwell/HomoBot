#ifndef UC_COMMON__FORCE_CONTROLLER_HPP_
#define UC_COMMON__FORCE_CONTROLLER_HPP_

#include "geometry_msgs/msg/wrench.hpp"
#include "geometry_msgs/msg/twist.hpp"

namespace uc_common
{

/**
 * @brief Admittance/impedance force controller
 */
class ForceController
{
public:
  ForceController();
  ~ForceController();

  bool init(
    double mass,
    double damping,
    double stiffness);

  /**
   * @brief Compute velocity adjustment from force error
   */
  void update(
    const geometry_msgs::msg::Wrench & force_error,
    double dt,
    geometry_msgs::msg::Twist & out_velocity_adjust);

  void reset();

private:
  bool initialized_ = false;
  double mass_ = 0.0;
  double damping_ = 0.0;
  double stiffness_ = 0.0;
};

}  // namespace uc_common

#endif  // UC_COMMON__FORCE_CONTROLLER_HPP_
