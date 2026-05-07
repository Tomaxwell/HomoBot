from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='data_quality_filter',
            executable='data_quality_filter_node',
            name='data_quality_filter_node',
            output='screen',
            parameters=[
                PathJoinSubstitution([
                    FindPackageShare('data_quality_filter'),
                    'config',
                    'data_quality_params.yaml'
                ])
            ],
        ),
    ])
