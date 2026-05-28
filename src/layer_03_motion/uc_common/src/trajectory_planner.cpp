#include "uc_common/trajectory_planner.hpp"

namespace uc_common
{

TrajectoryPlanner::TrajectoryPlanner() = default;
TrajectoryPlanner::~TrajectoryPlanner() = default;

bool TrajectoryPlanner::init(double max_velocity, double max_acceleration)
{
  max_velocity_ = max_velocity;
  max_acceleration_ = max_acceleration;
  initialized_ = true;
  return true;
}

void TrajectoryPlanner::plan(
  const geometry_msgs::msg::Pose & /*current*/,
  const geometry_msgs::msg::Pose & /*target*/,
  double /*dt*/,
  geometry_msgs::msg::Pose & /*out_next*/)
{
  // TODO: Implement trajectory planning
}

void TrajectoryPlanner::reset()
{
}

}  // namespace uc_common
