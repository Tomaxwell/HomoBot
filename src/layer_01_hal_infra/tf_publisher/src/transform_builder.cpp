#include "tf_publisher/transform_builder.hpp"

#include "urdf/model.h"

namespace tf_publisher
{

TransformBuilder::TransformBuilder() = default;
TransformBuilder::~TransformBuilder() = default;

bool TransformBuilder::init(const urdf::Model * model)
{
  if (!model) return false;
  model_ = model;
  initialized_ = true;
  return true;
}

std::vector<geometry_msgs::msg::TransformStamped> TransformBuilder::build_dynamic_transforms(
  const std::vector<double> & /*joint_positions*/)
{
  std::vector<geometry_msgs::msg::TransformStamped> transforms;
  if (!initialized_) return transforms;

  // TODO: Build transforms from URDF joints with current positions
  return transforms;
}

std::vector<geometry_msgs::msg::TransformStamped> TransformBuilder::build_static_transforms()
{
  std::vector<geometry_msgs::msg::TransformStamped> transforms;
  if (!initialized_) return transforms;

  // TODO: Build transforms for fixed joints
  return transforms;
}

}  // namespace tf_publisher
