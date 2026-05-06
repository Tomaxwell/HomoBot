#include "uc_common/self_collision_checker.hpp"

namespace uc_common
{

SelfCollisionChecker::SelfCollisionChecker() = default;
SelfCollisionChecker::~SelfCollisionChecker() = default;

bool SelfCollisionChecker::init(const std::vector<std::string> & joint_names)
{
  joint_names_ = joint_names;
  initialized_ = true;
  return true;
}

std::pair<bool, bool> SelfCollisionChecker::check(
  const std::vector<double> & /*positions*/)
{
  if (!initialized_) return {false, false};
  // TODO: Implement collision checking
  return {false, false};
}

std::string SelfCollisionChecker::get_collision_pair() const
{
  return collision_pair_;
}

void SelfCollisionChecker::reset()
{
  collision_pair_.clear();
}

}  // namespace uc_common
