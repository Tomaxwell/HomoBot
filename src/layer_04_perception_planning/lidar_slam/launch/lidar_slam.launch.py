from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('lidar_slam'),
        'config',
        'lidar_slam_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='lidar_slam',
            executable='lidar_slam_node',
            name='lidar_slam_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
