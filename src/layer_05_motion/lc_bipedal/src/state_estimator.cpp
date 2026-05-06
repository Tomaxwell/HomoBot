#include "lc_bipedal/state_estimator.hpp"

namespace lc_bipedal
{

StateEstimator::StateEstimator() = default;
StateEstimator::~StateEstimator() = default;

bool StateEstimator::init(size_t num_joints)
{
  num_joints_ = num_joints;
  initialized_ = true;
  return true;
}

void StateEstimator::update(
  const std::vector<double> & /*joint_positions*/,
  const std::vector<double> & /*joint_velocities*/)
{
  // TODO: Forward kinematics for foot positions
}

void StateEstimator::get_foot_positions(
  std::vector<double> & out_left_foot,
  std::vector<double> & out_right_foot) const
{
  out_left_foot = {0.0, 0.1, 0.0};
  out_right_foot = {0.0, -0.1, 0.0};
}

void StateEstimator::get_support_polygon(std::vector<double> & out_polygon) const
{
  out_polygon = {0.0, 0.1, 0.0, -0.1, 0.0, 0.0};
}

void StateEstimator::reset()
{
}

}  // namespace lc_bipedal
