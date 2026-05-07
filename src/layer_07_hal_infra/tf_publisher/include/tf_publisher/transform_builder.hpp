#ifndef TF_PUBLISHER__TRANSFORM_BUILDER_HPP_
#define TF_PUBLISHER__TRANSFORM_BUILDER_HPP_

#include <vector>
#include <string>

#include "geometry_msgs/msg/transform_stamped.hpp"

namespace urdf
{
class Model;
}

namespace tf_publisher
{

/**
 * @brief Transform builder from URDF model
 *
 * Extracts joint transforms from URDF and builds
 * geometry_msgs::TransformStamped messages.
 */
class TransformBuilder
{
public:
  TransformBuilder();
  ~TransformBuilder();

  bool init(const urdf::Model * model);

  /**
   * @brief Build all non-fixed joint transforms
   */
  std::vector<geometry_msgs::msg::TransformStamped> build_dynamic_transforms(
    const std::vector<double> & joint_positions);

  /**
   * @brief Build all fixed joint transforms
   */
  std::vector<geometry_msgs::msg::TransformStamped> build_static_transforms();

private:
  bool initialized_ = false;
  const urdf::Model * model_ = nullptr;
};

}  // namespace tf_publisher

#endif  // TF_PUBLISHER__TRANSFORM_BUILDER_HPP_
