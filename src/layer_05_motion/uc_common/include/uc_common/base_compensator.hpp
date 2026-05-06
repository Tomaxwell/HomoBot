#ifndef UC_COMMON__BASE_COMPENSATOR_HPP_
#define UC_COMMON__BASE_COMPENSATOR_HPP_

#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/pose.hpp"

namespace uc_common
{

/**
 * @brief Base disturbance compensator
 *
 * Compensates for base motion disturbances to maintain
 * end-effector tracking accuracy.
 */
class BaseCompensator
{
public:
  BaseCompensator();
  ~BaseCompensator();

  bool init(double bandwidth);

  /**
   * @brief Compute pose adjustment from base motion
   */
  void compensate(
    const geometry_msgs::msg::Twist & base_velocity,
    const geometry_msgs::msg::Pose & target_pose,
    geometry_msgs::msg::Pose & out_adjusted_pose);

  void reset();

private:
  bool initialized_ = false;
  double bandwidth_ = 0.0;
};

}  // namespace uc_common

#endif  // UC_COMMON__BASE_COMPENSATOR_HPP_
