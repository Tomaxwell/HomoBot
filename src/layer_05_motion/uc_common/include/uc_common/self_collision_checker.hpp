#ifndef UC_COMMON__SELF_COLLISION_CHECKER_HPP_
#define UC_COMMON__SELF_COLLISION_CHECKER_HPP_

#include <vector>
#include <string>

namespace uc_common
{

/**
 * @brief Self-collision detection and avoidance
 */
class SelfCollisionChecker
{
public:
  SelfCollisionChecker();
  ~SelfCollisionChecker();

  bool init(const std::vector<std::string> & joint_names);

  /**
   * @brief Check if current configuration is in collision
   * @return pair of (warning, critical) flags
   */
  std::pair<bool, bool> check(
    const std::vector<double> & positions);

  /**
   * @brief Get the name of colliding joint pair
   */
  std::string get_collision_pair() const;

  void reset();

private:
  bool initialized_ = false;
  std::vector<std::string> joint_names_;
  std::string collision_pair_;
};

}  // namespace uc_common

#endif  // UC_COMMON__SELF_COLLISION_CHECKER_HPP_
