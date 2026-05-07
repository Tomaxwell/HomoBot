#include "uc_common/ik_solver.hpp"

namespace uc_common
{

IKSolver::IKSolver() = default;
IKSolver::~IKSolver() = default;

bool IKSolver::init(const std::vector<std::string> & joint_names)
{
  joint_names_ = joint_names;
  initialized_ = true;
  return true;
}

bool IKSolver::solve(
  const std::string & /*side*/,
  const geometry_msgs::msg::Pose & /*target_pose*/,
  const std::vector<double> & /*current_positions*/,
  std::vector<double> & /*out_positions*/)
{
  if (!initialized_) return false;
  // TODO: Implement IK solver
  return true;
}

void IKSolver::reset()
{
}

}  // namespace uc_common
