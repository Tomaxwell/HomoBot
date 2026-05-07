from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='data_rule_engine',
            executable='data_rule_engine_node',
            name='data_rule_engine_node',
            output='screen',
            parameters=[
                PathJoinSubstitution([
                    FindPackageShare('data_rule_engine'),
                    'config',
                    'data_rule_engine_params.yaml'
                ])
            ],
        ),
    ])
