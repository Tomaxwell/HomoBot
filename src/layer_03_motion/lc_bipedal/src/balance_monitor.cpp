#include "lc_bipedal/balance_monitor.hpp"

#include <math>

namespace lc_bipedal
{

BalanceMonitor::BalanceMonitor() = default;
BalanceMonitor::~BalanceMonitor() = default;

bool BalanceMonitor::init(double com_threshold, double angular_threshold)
{
  com_threshold_ = com_threshold;
  angular_threshold_ = angular_threshold;
  initialized_ = true;
  return true;
}

double BalanceMonitor::check(
  const std::vector<double> & /*com_position*/,
  const std::vector<double> & /*support_polygon*/,
  double base_roll,
  double base_pitch)
{
  if (!initialized_) return 0.0;

  // Simple check: if roll or pitch exceeds threshold, flag falling
  if (std::abs(base_roll) > angular_threshold_ ||
      std::abs(base_pitch) > angular_threshold_)
  {
    falling_detected_ = true;
    return 0.0;
  }

  falling_detected_ = false;
  balance_lost_ = false;
  return 1.0;
}

void BalanceMonitor::reset()
{
  falling_detected_ = false;
  balance_lost_ = false;
}

}  // namespace lc_bipedal
