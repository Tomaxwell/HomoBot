#ifndef LC_BIPEDAL__STATE_ESTIMATOR_HPP_
#define LC_BIPEDAL__STATE_ESTIMATOR_HPP_

#include <vector>

namespace lc_bipedal
{

/**
 * @brief Lower body state estimator
 *
 * Computes foot positions via forward kinematics and
 * support polygon from contact states.
 */
class StateEstimator
{
public:
  StateEstimator();
  ~StateEstimator();

  bool init(size_t num_joints);

  /**
   * @brief Update lower body kinematic state
   */
  void update(
    const std::vector<double> & joint_positions,
    const std::vector<double> & joint_velocities);

  void get_foot_positions(
    std::vector<double> & out_left_foot,
    std::vector<double> & out_right_foot) const;

  void get_support_polygon(std::vector<double> & out_polygon) const;

  void reset();

private:
  bool initialized_ = false;
  size_t num_joints_ = 0;
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__STATE_ESTIMATOR_HPP_
