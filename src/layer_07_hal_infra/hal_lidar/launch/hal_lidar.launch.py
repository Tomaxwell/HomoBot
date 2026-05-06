from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('hal_lidar'),
        'config',
        'hal_lidar_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='hal_lidar',
            executable='hal_lidar_node',
            name='hal_lidar_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
