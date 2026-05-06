#include "lc_wheeled/lift_controller.hpp"

#include <math>

namespace lc_wheeled
{

LiftController::LiftController() = default;
LiftController::~LiftController() = default;

bool LiftController::init(
  double min_height,
  double max_height,
  double max_velocity,
  double kp,
  double ki,
  double kd)
{
  min_height_ = min_height;
  max_height_ = max_height;
  max_velocity_ = max_velocity;
  kp_ = kp;
  ki_ = ki;
  kd_ = kd;
  initialized_ = true;
  return true;
}

bool LiftController::update(
  double target_height,
  double current_height,
  double dt,
  double & out_velocity)
{
  if (!initialized_) {
    out_velocity = 0.0;
    return false;
  }

  // Clamp target to limits
  target_height = std::max(min_height_, std::min(max_height_, target_height));

  double error = target_height - current_height;
  integral_ += error * dt;
  double derivative = (error - prev_error_) / dt;

  out_velocity = kp_ * error + ki_ * integral_ + kd_ * derivative;
  out_velocity = std::max(-max_velocity_, std::min(max_velocity_, out_velocity));

  prev_error_ = error;
  in_position_ = std::abs(error) < 0.001;  // 1mm tolerance

  return true;
}

void LiftController::reset()
{
  integral_ = 0.0;
  prev_error_ = 0.0;
  in_position_ = true;
}

}  // namespace lc_wheeled
