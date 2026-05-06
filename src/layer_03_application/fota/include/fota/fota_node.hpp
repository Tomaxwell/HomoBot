#ifndef FOTA__FOTANODE_HPP_
#define FOTA__FOTANODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace fota
{

class FotaNode : public rclcpp::Node
{
public:
  explicit FotaNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~FotaNode() override;

private:
};

} // namespace fota

#endif // FOTA__FOTANODE_HPP_
