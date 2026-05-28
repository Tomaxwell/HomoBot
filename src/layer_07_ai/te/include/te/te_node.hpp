#ifndef TE__TASKENGINENODE_HPP_
#define TE__TASKENGINENODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace te
{

class TaskEngineNode : public rclcpp::Node
{
public:
  explicit TaskEngineNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~TaskEngineNode() override;

private:
};

} // namespace te

#endif // TE__TASKENGINENODE_HPP_
