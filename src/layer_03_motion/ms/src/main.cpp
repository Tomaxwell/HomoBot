#include <rclcpp/rclcpp.hpp>
#include "ms/ms_node.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<ms::MotionStreamerNode>();
  rclcpp::spin(node);

  rclcpp::shutdown();
  return 0;
}
