from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='data_uploader',
            executable='data_uploader_node',
            name='data_uploader_node',
            output='screen',
            parameters=[
                PathJoinSubstitution([
                    FindPackageShare('data_uploader'),
                    'config',
                    'data_uploader_params.yaml'
                ])
            ],
        ),
    ])
