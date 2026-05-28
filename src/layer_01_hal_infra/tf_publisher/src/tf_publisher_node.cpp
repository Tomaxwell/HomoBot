#include "tf_publisher/tf_publisher_node.hpp"

#include "tf_publisher/model_manager.hpp"
#include "tf_publisher/transform_builder.hpp"
#include "tf_publisher/frame_graph_monitor.hpp"

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "tf_msgs/srv/reload_robot_description.hpp"

namespace tf_publisher
{

TFPublisherNode::TFPublisherNode(const rclcpp::NodeOptions & options)
: Node("tf_publisher_node", options)
{
  // Parameters
  publish_rate_ = this->declare_parameter<double>("publish_rate", 100.0);

  // Components
  model_manager_ = std::make_unique<ModelManager>();
  transform_builder_ = std::make_unique<TransformBuilder>();
  frame_graph_monitor_ = std::make_unique<FrameGraphMonitor>();

  // Broadcasters
  tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
  static_tf_broadcaster_ = std::make_unique<tf2_ros::StaticTransformBroadcaster>(*this);

  // Subscribe to robot_description
  robot_description_sub_ = this->create_subscription<std_msgs::msg::String>(
    "/robot_description",
    rclcpp::QoS(1).transient_local(),
    [this](const std_msgs::msg::String::SharedPtr msg) {
      robot_description_ = msg->data;
      this->load_robot_description();
    });

  // Service to reload URDF
  reload_service_ = this->create_service<tf_msgs::srv::ReloadRobotDescription>(
    "/tf/reload_robot_description",
    std::bind(
      &TFPublisherNode::handle_reload_request, this,
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  // Timers
  publish_timer_ = this->create_wall_timer(
    std::chrono::milliseconds(static_cast<int>(1000.0 / publish_rate_)),
    std::bind(&TFPublisherNode::publish_transforms, this));

  heartbeat_timer_ = this->create_wall_timer(
    std::chrono::seconds(1),
    std::bind(&TFPublisherNode::publish_heartbeat, this));

  RCLCPP_INFO(this->get_logger(), "TFPublisherNode initialized");
}

TFPublisherNode::~TFPublisherNode() = default;

void TFPublisherNode::load_robot_description()
{
  if (robot_description_.empty()) {
    RCLCPP_WARN(this->get_logger(), "Empty robot_description");
    return;
  }

  if (!model_manager_>load_from_string(robot_description_)) {
    RCLCPP_ERROR(this->get_logger(), "Failed to load URDF model");
    return;
  }

  if (!transform_builder_>init(model_manager_>get_model())) {
    RCLCPP_ERROR(this->get_logger(), "Failed to init TransformBuilder");
    return;
  }

  // Publish static transforms once
  auto static_transforms = transform_builder_>build_static_transforms();
  if (!static_transforms.empty()) {
    static_tf_broadcaster_>sendTransform(static_transforms);
  }

  initialized_ = true;
  RCLCPP_INFO(this->get_logger(), "Robot model loaded, %zu static frames",
    static_transforms.size());
}

void TFPublisherNode::publish_transforms()
{
  if (!initialized_) {
    return;
  }

  // TODO: Get current joint positions from JointState
  // For now, publish with zero positions
  std::vector<double> joint_positions;
  auto transforms = transform_builder_>build_dynamic_transforms(joint_positions);

  if (!transforms.empty()) {
    tf_broadcaster_>sendTransform(transforms);
  }
}

void TFPublisherNode::publish_heartbeat()
{
  // TODO: Publish tf_msgs/Heartbeat
}

void TFPublisherNode::handle_reload_request(
  const std::shared_ptr<rmw_request_id_t> /*request_header*/,
  const std::shared_ptr<tf_msgs::srv::ReloadRobotDescription::Request> request,
  std::shared_ptr<tf_msgs::srv::ReloadRobotDescription::Response> response)
{
  robot_description_ = request->urdf_string;
  load_robot_description();
  response->success = initialized_;
}

}  // namespace tf_publisher

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(tf_publisher::TFPublisherNode)
