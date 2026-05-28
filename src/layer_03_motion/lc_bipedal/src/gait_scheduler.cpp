#include "lc_bipedal/gait_scheduler.hpp"

namespace lc_bipedal
{

GaitScheduler::GaitScheduler() = default;
GaitScheduler::~GaitScheduler() = default;

bool GaitScheduler::init(double gait_period, double duty_factor)
{
  gait_period_ = gait_period;
  duty_factor_ = duty_factor;
  initialized_ = true;
  return true;
}

void GaitScheduler::update(
  double dt,
  double /*velocity_command*/,
  bool /*contact_left*/,
  bool /*contact_right*/)
{
  if (!initialized_) return;

  phase_ += dt / gait_period_;
  if (phase_ >= 1.0) {
    phase_ -= 1.0;
  }

  // TODO: Implement full gait FSM transitions
  // Simplified: use phase to determine swing/stance
}

bool GaitScheduler::is_left_swing() const
{
  return phase_ < (1.0 - duty_factor_);
}

bool GaitScheduler::is_right_swing() const
{
  return phase_ >= duty_factor_;
}

void GaitScheduler::reset()
{
  phase_ = 0.0;
  current_state_ = State::GROUND_IDLE;
}

}  // namespace lc_bipedal
