#ifndef SETTING__SETTINGNODE_HPP_
#define SETTING__SETTINGNODE_HPP_

#include <rclcpp/rclcpp.hpp>

namespace setting
{

class SettingNode : public rclcpp::Node
{
public:
  explicit SettingNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~SettingNode() override;

private:
};

} // namespace setting

#endif // SETTING__SETTINGNODE_HPP_
