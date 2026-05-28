from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('dr'),
        'config',
        'dr_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='dr',
            executable='dr_node',
            name='dr_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
