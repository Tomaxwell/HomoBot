#ifndef LC_WHEELED__CASTER_MONITOR_HPP_
#define LC_WHEELED__CASTER_MONITOR_HPP_

#include <vector>
#include <string>

namespace lc_wheeled
{

/**
 * @brief Caster wheel monitor
 *
 * Monitors caster wheel states (orientation, fault) for
 * wheeled chassis safety.
 */
class CasterMonitor
{
public:
  CasterMonitor();
  ~CasterMonitor();

  bool init(size_t num_casters);

  /**
   * @brief Update caster state monitoring
   */
  void update(
    const std::vector<double> & caster_angles,
    const std::vector<double> & caster_velocities);

  bool has_fault() const { return fault_detected_; }
  std::string get_fault_description() const { return fault_desc_; }

  void reset();

private:
  bool initialized_ = false;
  size_t num_casters_ = 0;
  bool fault_detected_ = false;
  std::string fault_desc_;
};

}  // namespace lc_wheeled

#endif  // LC_WHEELED__CASTER_MONITOR_HPP_
