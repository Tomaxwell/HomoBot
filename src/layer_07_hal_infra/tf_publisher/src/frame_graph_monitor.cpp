#include "tf_publisher/frame_graph_monitor.hpp"

namespace tf_publisher
{

FrameGraphMonitor::FrameGraphMonitor() = default;
FrameGraphMonitor::~FrameGraphMonitor() = default;

void FrameGraphMonitor::update_frame_list(const std::vector<std::string> & frame_ids)
{
  // TODO: Detect missing frames, cycles, disconnected subgraphs
  known_frames_ = frame_ids;
  anomaly_detected_ = false;
}

void FrameGraphMonitor::reset()
{
  anomaly_detected_ = false;
  anomaly_desc_.clear();
  known_frames_.clear();
}

}  // namespace tf_publisher
