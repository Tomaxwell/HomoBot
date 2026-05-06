from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('hal_camera'),
        'config',
        'hal_camera_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='hal_camera',
            executable='hal_camera_node',
            name='hal_camera_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
