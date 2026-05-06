from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('setting'),
        'config',
        'setting_node.yaml'
    ])

    return LaunchDescription([
        Node(
            package='setting',
            executable='setting_node',
            name='setting_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
