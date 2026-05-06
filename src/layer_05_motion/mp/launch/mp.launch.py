from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('mp'),
        'config',
        'mp_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='mp',
            executable='mp_node',
            name='mp_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
