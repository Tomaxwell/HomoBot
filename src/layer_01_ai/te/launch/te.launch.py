from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('te'),
        'config',
        'te_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='te',
            executable='te_node',
            name='te_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
