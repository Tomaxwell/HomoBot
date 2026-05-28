#ifndef EM__EXECUTIVEMANAGERNODE_HPP_
#define EM__EXECUTIVEMANAGERNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace em
{

class ExecutiveManagerNode : public rclcpp::Node
{
public:
  explicit ExecutiveManagerNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~ExecutiveManagerNode() override;

private:
};

} // namespace em

#endif // EM__EXECUTIVEMANAGERNODE_HPP_
