#include "lc_wheeled/chassis_controller.hpp"

#include <math>

namespace lc_wheeled
{

ChassisController::ChassisController() = default;
ChassisController::~ChassisController() = default;

bool ChassisController::init(
  double wheel_radius,
  double wheel_base,
  double max_linear_speed,
  double max_angular_speed)
{
  wheel_radius_ = wheel_radius;
  wheel_base_ = wheel_base;
  max_linear_speed_ = max_linear_speed;
  max_angular_speed_ = max_angular_speed;
  initialized_ = true;
  return true;
}

void ChassisController::update(
  double vx,
  double yaw_rate,
  double & out_left_wheel_vel,
  double & out_right_wheel_vel)
{
  if (!initialized_) {
    out_left_wheel_vel = 0.0;
    out_right_wheel_vel = 0.0;
    return;
  }

  // Clamp inputs
  vx = std::max(-max_linear_speed_, std::min(max_linear_speed_, vx));
  yaw_rate = std::max(-max_angular_speed_, std::min(max_angular_speed_, yaw_rate));

  // Differential drive kinematics
  // v_left = (vx - yaw_rate * wheel_base / 2) / wheel_radius
  // v_right = (vx + yaw_rate * wheel_base / 2) / wheel_radius
  out_left_wheel_vel = (vx - yaw_rate * wheel_base_ / 2.0) / wheel_radius_;
  out_right_wheel_vel = (vx + yaw_rate * wheel_base_ / 2.0) / wheel_radius_;
}

void ChassisController::reset()
{
  // Nothing to reset for pure kinematics
}

}  // namespace lc_wheeled
