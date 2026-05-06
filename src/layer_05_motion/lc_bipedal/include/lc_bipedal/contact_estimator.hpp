#ifndef LC_BIPEDAL__CONTACT_ESTIMATOR_HPP_
#define LC_BIPEDAL__CONTACT_ESTIMATOR_HPP_

#include <vector>
#include <math>

namespace lc_bipedal
{

/**
 * @brief Ground contact force estimator via inverse dynamics
 *
 * Estimates foot contact forces from joint torques and IMU data.
 * No foot force sensors available.
 */
class ContactEstimator
{
public:
  ContactEstimator();
  ~ContactEstimator();

  bool init(size_t num_joints);

  /**
   * @brief Estimate contact forces
   * @param joint_positions Joint positions [rad]
   * @param joint_velocities Joint velocities [rad/s]
   * @param joint_torques Joint torques [Nm]
   * @param base_acceleration Base linear acceleration [m/s^2]
   * @param out_left_force Left foot contact force [N] (3D)
   * @param out_right_force Right foot contact force [N] (3D)
   * @return true if estimation succeeded
   */
  bool estimate(
    const std::vector<double> & joint_positions,
    const std::vector<double> & joint_velocities,
    const std::vector<double> & joint_torques,
    const std::vector<double> & base_acceleration,
    std::vector<double> & out_left_force,
    std::vector<double> & out_right_force);

  void reset();

private:
  bool initialized_ = false;
  size_t num_joints_ = 0;
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__CONTACT_ESTIMATOR_HPP_
