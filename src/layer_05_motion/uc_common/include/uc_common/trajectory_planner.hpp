#ifndef UC_COMMON__TRAJECTORY_PLANNER_HPP_
#define UC_COMMON__TRAJECTORY_PLANNER_HPP_

#include <string>
#include <vector>

#include "geometry_msgs/msg/pose.hpp"

namespace uc_common
{

/**
 * @brief Trajectory planner for smooth end-effector motion
 */
class TrajectoryPlanner
{
public:
  TrajectoryPlanner();
  ~TrajectoryPlanner();

  bool init(double max_velocity, double max_acceleration);

  /**
   * @brief Plan a trajectory from current to target pose
   */
  void plan(
    const geometry_msgs::msg::Pose & current,
    const geometry_msgs::msg::Pose & target,
    double dt,
    geometry_msgs::msg::Pose & out_next);

  void reset();

private:
  bool initialized_ = false;
  double max_velocity_ = 0.0;
  double max_acceleration_ = 0.0;
};

}  // namespace uc_common

#endif  // UC_COMMON__TRAJECTORY_PLANNER_HPP_
