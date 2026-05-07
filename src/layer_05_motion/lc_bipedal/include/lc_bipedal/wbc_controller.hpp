#ifndef LC_BIPEDAL__WBC_CONTROLLER_HPP_
#define LC_BIPEDAL__WBC_CONTROLLER_HPP_

#include <vector>
#include <math>

namespace lc_bipedal
{

/**
 * @brief QP-based Whole Body Controller
 *
 * Solves a QP problem using OSQP to track RL policy torques
 * while satisfying zero angular momentum and contact constraints.
 */
class WBCController
{
public:
  WBCController();
  ~WBCController();

  bool init(size_t num_joints, size_t num_contacts);

  /**
   * @brief Solve WBC QP
   * @param desired_torques RL policy output torques
   * @param contact_forces Estimated ground contact forces
   * @param base_state Current base orientation/velocity
   * @param out_joint_torques Final joint torques
   * @return true if QP solved successfully
   */
  bool solve(
    const std::vector<double> & desired_torques,
    const std::vector<double> & contact_forces,
    const std::vector<double> & base_state,
    std::vector<double> & out_joint_torques);

  void reset();

private:
  bool initialized_ = false;
  size_t num_joints_ = 0;
  size_t num_contacts_ = 0;
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__WBC_CONTROLLER_HPP_
