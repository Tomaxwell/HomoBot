#ifndef AGENT__AGENTNODE_HPP_
#define AGENT__AGENTNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace agent
{

class AgentNode : public rclcpp::Node
{
public:
  explicit AgentNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~AgentNode() override;

private:
};

} // namespace agent

#endif // AGENT__AGENTNODE_HPP_
