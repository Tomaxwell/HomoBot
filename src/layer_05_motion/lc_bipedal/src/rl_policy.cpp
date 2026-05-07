#include "lc_bipedal/rl_policy.hpp"

namespace lc_bipedal
{

RLPolicy::RLPolicy() = default;
RLPolicy::~RLPolicy() = default;

bool RLPolicy::init(const std::string & model_path)
{
  model_path_ = model_path;
  // TODO: Load ONNX model via ONNX Runtime
  initialized_ = true;
  return true;
}

bool RLPolicy::infer(
  const std::vector<float> & /*observation*/,
  std::vector<float> & /*out_action*/)
{
  if (!initialized_) return false;
  // TODO: Run ONNX inference
  return true;
}

bool RLPolicy::is_result_ready() const
{
  return true;
}

bool RLPolicy::get_latest_result(std::vector<float> & /*out_action*/)
{
  if (!initialized_) return false;
  // TODO: Return cached async result
  return true;
}

void RLPolicy::reset()
{
  std::fill(cached_action_.begin(), cached_action_.end(), 0.0f);
}

void RLPolicy::stop_inference_thread()
{
  inference_thread_running_ = false;
}

}  // namespace lc_bipedal
