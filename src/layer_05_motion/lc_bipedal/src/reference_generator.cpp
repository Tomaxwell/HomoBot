#include "lc_bipedal/reference_generator.hpp"

namespace lc_bipedal
{

ReferenceGenerator::ReferenceGenerator() = default;
ReferenceGenerator::~ReferenceGenerator() = default;

bool ReferenceGenerator::init(double com_height, double step_height)
{
  com_height_ = com_height;
  step_height_ = step_height;
  initialized_ = true;
  return true;
}

void ReferenceGenerator::generate(
  double /*vx*/,
  double /*vy*/,
  double /*yaw_rate*/,
  double /*gait_phase*/,
  std::vector<double> & out_com_ref,
  std::vector<double> & out_foot_left_ref,
  std::vector<double> & out_foot_right_ref)
{
  out_com_ref = {0.0, 0.0, com_height_};
  out_foot_left_ref = {0.0, 0.1, 0.0};
  out_foot_right_ref = {0.0, -0.1, 0.0};
}

void ReferenceGenerator::reset()
{
}

}  // namespace lc_bipedal
