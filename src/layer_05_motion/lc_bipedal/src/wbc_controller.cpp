#include "lc_bipedal/wbc_controller.hpp"

namespace lc_bipedal
{

WBCController::WBCController() = default;
WBCController::~WBCController() = default;

bool WBCController::init(size_t num_joints, size_t num_contacts)
{
  num_joints_ = num_joints;
  num_contacts_ = num_contacts;
  // TODO: Initialize OSQP solver
  initialized_ = true;
  return true;
}

bool WBCController::solve(
  const std::vector<double> & /*desired_torques*/,
  const std::vector<double> & /*contact_forces*/,
  const std::vector<double> & /*base_state*/,
  std::vector<double> & /*out_joint_torques*/)
{
  if (!initialized_) return false;
  // TODO: Build and solve QP using OSQP
  return true;
}

void WBCController::reset()
{
  // TODO: Clear OSQP warm-start
}

}  // namespace lc_bipedal
