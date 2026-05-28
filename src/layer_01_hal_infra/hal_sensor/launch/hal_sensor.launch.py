from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = PathJoinSubstitution([
        FindPackageShare('hal_sensor'),
        'config',
        'hal_sensor_params.yaml'
    ])

    return LaunchDescription([
        Node(
            package='hal_sensor',
            executable='hal_sensor_node',
            name='hal_sensor_node',
            output='screen',
            parameters=[config_file],
            emulate_tty=True,
        )
    ])
