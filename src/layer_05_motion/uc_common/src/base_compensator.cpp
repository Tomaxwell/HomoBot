#include "uc_common/base_compensator.hpp"

namespace uc_common
{

BaseCompensator::BaseCompensator() = default;
BaseCompensator::~BaseCompensator() = default;

bool BaseCompensator::init(double bandwidth)
{
  bandwidth_ = bandwidth;
  initialized_ = true;
  return true;
}

void BaseCompensator::compensate(
  const geometry_msgs::msg::Twist & /*base_velocity*/,
  const geometry_msgs::msg::Pose & /*target_pose*/,
  geometry_msgs::msg::Pose & /*out_adjusted_pose*/)
{
  // TODO: Implement base disturbance compensation
}

void BaseCompensator::reset()
{
}

}  // namespace uc_common
