from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('em'),
        'config',
        'em_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='em',
            executable='em_node',
            name='em_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
