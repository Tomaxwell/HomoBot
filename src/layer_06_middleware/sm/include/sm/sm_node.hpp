#ifndef SM__STATEMANAGERNODE_HPP_
#define SM__STATEMANAGERNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace sm
{

class StateManagerNode : public rclcpp::Node
{
public:
  explicit StateManagerNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~StateManagerNode() override;

private:
};

} // namespace sm

#endif // SM__STATEMANAGERNODE_HPP_
