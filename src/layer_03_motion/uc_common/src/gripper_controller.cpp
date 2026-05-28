#include "uc_common/gripper_controller.hpp"

namespace uc_common
{

GripperController::GripperController() = default;
GripperController::~GripperController() = default;

bool GripperController::init(double max_width, double max_effort)
{
  max_width_ = max_width;
  max_effort_ = max_effort;
  initialized_ = true;
  return true;
}

void GripperController::set_target(const std::string & /*side*/, double /*position*/, double /*max_effort*/)
{
}

void GripperController::open(const std::string & /*side*/)
{
}

void GripperController::close(const std::string & /*side*/)
{
}

double GripperController::get_position(const std::string & /*side*/) const
{
  return 0.0;
}

bool GripperController::is_closed(const std::string & /*side*/) const
{
  return false;
}

void GripperController::reset()
{
}

}  // namespace uc_common
