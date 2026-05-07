#include "lc_wheeled/caster_monitor.hpp"

namespace lc_wheeled
{

CasterMonitor::CasterMonitor() = default;
CasterMonitor::~CasterMonitor() = default;

bool CasterMonitor::init(size_t num_casters)
{
  num_casters_ = num_casters;
  initialized_ = true;
  return true;
}

void CasterMonitor::update(
  const std::vector<double> & /*caster_angles*/,
  const std::vector<double> & /*caster_velocities*/)
{
  if (!initialized_) return;
  // TODO: Implement caster fault detection
  fault_detected_ = false;
}

void CasterMonitor::reset()
{
  fault_detected_ = false;
  fault_desc_.clear();
}

}  // namespace lc_wheeled
