from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('hal_ethercat'),
        'config',
        'hal_ethercat_node.yaml'
    ])

    return LaunchDescription([
        Node(
            package='hal_ethercat',
            executable='hal_ethercat_node',
            name='hal_ethercat_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
