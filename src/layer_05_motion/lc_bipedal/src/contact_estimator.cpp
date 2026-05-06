#include "lc_bipedal/contact_estimator.hpp"

namespace lc_bipedal
{

ContactEstimator::ContactEstimator() = default;
ContactEstimator::~ContactEstimator() = default;

bool ContactEstimator::init(size_t num_joints)
{
  num_joints_ = num_joints;
  initialized_ = true;
  return true;
}

bool ContactEstimator::estimate(
  const std::vector<double> & /*joint_positions*/,
  const std::vector<double> & /*joint_velocities*/,
  const std::vector<double> & /*joint_torques*/,
  const std::vector<double> & /*base_acceleration*/,
  std::vector<double> & out_left_force,
  std::vector<double> & out_right_force)
{
  if (!initialized_) return false;
  // TODO: Implement inverse dynamics contact estimation
  out_left_force.resize(3, 0.0);
  out_right_force.resize(3, 0.0);
  return true;
}

void ContactEstimator::reset()
{
}

}  // namespace lc_bipedal
