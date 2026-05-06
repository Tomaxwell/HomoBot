#ifndef TF_PUBLISHER__FRAME_GRAPH_MONITOR_HPP_
#define TF_PUBLISHER__FRAME_GRAPH_MONITOR_HPP_

#include <vector>
#include <string>

namespace tf_publisher
{

/**
 * @brief Frame graph health monitor
 *
 * Detects anomalies in the TF tree (missing frames,
 * cycles, disconnected subgraphs).
 */
class FrameGraphMonitor
{
public:
  FrameGraphMonitor();
  ~FrameGraphMonitor();

  void update_frame_list(const std::vector<std::string> & frame_ids);

  bool has_anomaly() const { return anomaly_detected_; }
  std::string get_anomaly_description() const { return anomaly_desc_; }

  void reset();

private:
  bool anomaly_detected_ = false;
  std::string anomaly_desc_;
  std::vector<std::string> known_frames_;
};

}  // namespace tf_publisher

#endif  // TF_PUBLISHER__FRAME_GRAPH_MONITOR_HPP_
