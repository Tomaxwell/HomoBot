#ifndef PNC__PNCNODE_HPP_
#define PNC__PNCNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace pnc
{

class PncNode : public rclcpp::Node
{
public:
  explicit PncNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~PncNode() override;

private:
};

} // namespace pnc

#endif // PNC__PNCNODE_HPP_
