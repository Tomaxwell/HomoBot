#ifndef MAPMANAGER__MAPMANAGERNODE_HPP_
#define MAPMANAGER__MAPMANAGERNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace mapmanager
{

class MapManagerNode : public rclcpp::Node
{
public:
  explicit MapManagerNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~MapManagerNode() override;

private:
};

} // namespace mapmanager

#endif // MAPMANAGER__MAPMANAGERNODE_HPP_
