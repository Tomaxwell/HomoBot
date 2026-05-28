from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('rc'),
        'config',
        'rc_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='rc',
            executable='rc_node',
            name='rc_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
