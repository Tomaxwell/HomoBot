#ifndef LC_BIPEDAL__RL_POLICY_HPP_
#define LC_BIPEDAL__RL_POLICY_HPP_

#include <vector>
#include <string>
#include <math>

namespace lc_bipedal
{

/**
 * @brief RL Policy inference wrapper (ONNX Runtime)
 *
 * Loads a PPO-trained MLP policy and runs inference.
 * Supports both synchronous and asynchronous inference modes.
 */
class RLPolicy
{
public:
  RLPolicy();
  ~RLPolicy();

  /**
   * @brief Load ONNX model
   * @param model_path Path to .onnx file
   * @return true on success
   */
  bool init(const std::string & model_path);

  /**
   * @brief Run inference synchronously
   * @param observation Observation vector (normalized)
 * @param out_action Output action vector (joint torques + contact forces)
   * @return true if inference succeeded
   */
  bool infer(
    const std::vector<float> & observation,
    std::vector<float> & out_action);

  /**
   * @brief Check if async inference result is ready
   */
  bool is_result_ready() const;

  /**
   * @brief Get latest async result
   */
  bool get_latest_result(std::vector<float> & out_action);

  void reset();
  void stop_inference_thread();

private:
  bool initialized_ = false;
  bool use_async_ = false;
  std::string model_path_;

  // Async inference thread
  bool inference_thread_running_ = false;
  std::vector<float> cached_action_;
};

}  // namespace lc_bipedal

#endif  // LC_BIPEDAL__RL_POLICY_HPP_
