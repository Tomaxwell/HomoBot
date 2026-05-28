#ifndef LC_BIPEDAL__GAIT_SCHEDULER_HPP_
#define LC_BIPEDAL__GAIT_SCHEDULER_HPP_

#include <cstdint>

namespace lc_bipedal
{

/**
 * @brief Gait FSM scheduler
 *
 * States: GROUND_IDLE / DOUBLE_SUPPORT / LEFT_SINGLE_SUPPORT /
 *         RIGHT_SINGLE_SUPPORT / LEFT_SWING / RIGHT_SWING /
 *         FALLING_RECOVERY / GROUND_FAULT
 */
class GaitScheduler
{
public:
  enum class State : uint8_t
  {
    GROUND_IDLE = 0,
    DOUBLE_SUPPORT,
    LEFT_SINGLE_SUPPORT,
    RIGHT_SINGLE_SUPPORT,
    LEFT_SWING,
    RIGHT_SWING,
    FALLING_RECOVERY,
    GROUND_FAULT
  };

  GaitScheduler();
  ~GaitScheduler();

  bool init(double gait_period, double duty_factor);

  /**
   * @brief Update gait state machine
   * @param dt Time step [s]
   * @param velocity_command Forward velocity command [m/s]
   * @param contact_left Left foot contact detected
   * @param contact_right Right foot contact detected
   */
  void update(
    double dt,
    double velocity_command,
    bool contact_left,
    bool contact_right);

  State get_state() const { return current_state_; }
  double get_phase() const { return phase_; }
  bool is_left_swing() const;
  bool is_right_swing() const;

  void reset();

private:
  bool initialized_ = false;
  double gait_period_ = 0.8;   // s
  double duty_factor_ = 0.6;
  double phase_ = 0.0;
  State current_state_ = State::GROUND_IDLE;
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__GAIT_SCHEDULER_HPP_
