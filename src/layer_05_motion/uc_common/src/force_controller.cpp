#include "uc_common/force_controller.hpp"

namespace uc_common
{

ForceController::ForceController() = default;
ForceController::~ForceController() = default;

bool ForceController::init(double mass, double damping, double stiffness)
{
  mass_ = mass;
  damping_ = damping;
  stiffness_ = stiffness;
  initialized_ = true;
  return true;
}

void ForceController::update(
  const geometry_msgs::msg::Wrench & /*force_error*/,
  double /*dt*/,
  geometry_msgs::msg::Twist & /*out_velocity_adjust*/)
{
  // TODO: Implement admittance/impedance control
}

void ForceController::reset()
{
}

}  // namespace uc_common
