#ifndef UC_COMMON__IK_SOLVER_HPP_
#define UC_COMMON__IK_SOLVER_HPP_

#include <string>
#include <vector>

#include "geometry_msgs/msg/pose.hpp"

namespace uc_common
{

/**
 * @brief Inverse Kinematics solver wrapper
 */
class IKSolver
{
public:
  IKSolver();
  ~IKSolver();

  bool init(const std::vector<std::string> & joint_names);

  /**
   * @brief Solve IK for a given end-effector pose
   * @param side "left" or "right"
   * @param target_pose Target pose in base frame
   * @param current_positions Current joint positions
   * @param out_positions Output joint positions
   * @return true if solution found
   */
  bool solve(
    const std::string & side,
    const geometry_msgs::msg::Pose & target_pose,
    const std::vector<double> & current_positions,
    std::vector<double> & out_positions);

  void reset();

private:
  bool initialized_ = false;
  std::vector<std::string> joint_names_;
};

}  // namespace uc_common

#endif  // UC_COMMON__IK_SOLVER_HPP_
