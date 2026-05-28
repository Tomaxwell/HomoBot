#ifndef LC_BIPEDAL__REFERENCE_GENERATOR_HPP_
#define LC_BIPEDAL__REFERENCE_GENERATOR_HPP_

#include <vector>

namespace lc_bipedal
{

/**
 * @brief Reference trajectory generator
 *
 * Generates desired CoM trajectory, foot trajectory, and swing foot
 * targets from velocity commands.
 */
class ReferenceGenerator
{
public:
  ReferenceGenerator();
  ~ReferenceGenerator();

  bool init(double com_height, double step_height);

  /**
   * @brief Generate reference trajectories
   * @param vx Forward velocity [m/s]
   * @param vy Lateral velocity [m/s]
   * @param yaw_rate Yaw rate [rad/s]
   * @param gait_phase Current gait phase [0, 1]
   * @param out_com_ref CoM reference position (3D)
   * @param out_foot_left_ref Left foot reference position (3D)
   * @param out_foot_right_ref Right foot reference position (3D)
   */
  void generate(
    double vx,
    double vy,
    double yaw_rate,
    double gait_phase,
    std::vector<double> & out_com_ref,
    std::vector<double> & out_foot_left_ref,
    std::vector<double> & out_foot_right_ref);

  void reset();

private:
  bool initialized_ = false;
  double com_height_ = 0.75;   // m
  double step_height_ = 0.08;  // m
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__REFERENCE_GENERATOR_HPP_
