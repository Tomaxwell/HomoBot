#ifndef LC_BIPEDAL__BALANCE_MONITOR_HPP_
#define LC_BIPEDAL__BALANCE_MONITOR_HPP_

#include <vector>

namespace lc_bipedal
{

/**
 * @brief Balance and fall detection monitor
 */
class BalanceMonitor
{
public:
  BalanceMonitor();
  ~BalanceMonitor();

  bool init(double com_threshold, double angular_threshold);

  /**
   * @brief Check balance status
   * @param com_position CoM position [m]
   * @param support_polygon Support polygon vertices
   * @param base_roll Roll angle [rad]
   * @param base_pitch Pitch angle [rad]
   * @return Balance score [0.0, 1.0]
   */
  double check(
    const std::vector<double> & com_position,
    const std::vector<double> & support_polygon,
    double base_roll,
    double base_pitch);

  bool is_falling() const { return falling_detected_; }
  bool is_balance_lost() const { return balance_lost_; }

  void reset();

private:
  bool initialized_ = false;
  double com_threshold_ = 0.02;       // m
  double angular_threshold_ = 0.3;    // rad
  bool falling_detected_ = false;
  bool balance_lost_ = false;
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__BALANCE_MONITOR_HPP_
