#ifndef UC_COMMON__GRIPPER_CONTROLLER_HPP_
#define UC_COMMON__GRIPPER_CONTROLLER_HPP_

#include <string>

namespace uc_common
{

/**
 * @brief Gripper controller
 */
class GripperController
{
public:
  GripperController();
  ~GripperController();

  bool init(double max_width, double max_effort);

  void set_target(const std::string & side, double position, double max_effort);
  void open(const std::string & side);
  void close(const std::string & side);

  double get_position(const std::string & side) const;
  bool is_closed(const std::string & side) const;

  void reset();

private:
  bool initialized_ = false;
  double max_width_ = 0.0;
  double max_effort_ = 0.0;
};

}  // namespace uc_common

#endif  // UC_COMMON__GRIPPER_CONTROLLER_HPP_
