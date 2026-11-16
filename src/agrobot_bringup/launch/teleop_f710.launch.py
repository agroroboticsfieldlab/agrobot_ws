# Drive the agrobot with a Logitech F710 gamepad (XInput mode).
# Hold LB to drive, LB + RB for turbo. Publishes TwistStamped on /agrobot/cmd_vel.
# bringup.launch.py already includes this file (use_teleop:=true by default); run it standalone only
# when the robot was started with use_teleop:=false, otherwise joy_node runs twice.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    declared_arguments = [
        DeclareLaunchArgument(
            "joy_config",
            default_value=PathJoinSubstitution([FindPackageShare("agrobot_bringup"), "config", "f710.yaml"]),
            description="Parameter file for joy_node and teleop_twist_joy_node.",
        ),
        DeclareLaunchArgument(
            "cmd_vel_topic",
            default_value="/agrobot/cmd_vel",
            description="Topic the TwistStamped commands are published on.",
        ),
    ]
    joy_config = LaunchConfiguration("joy_config")

    joy_node = Node(
        package="joy",
        executable="joy_node",
        name="joy_node",
        parameters=[joy_config],
        output="both",
    )

    teleop_node = Node(
        package="teleop_twist_joy",
        executable="teleop_node",
        name="teleop_twist_joy_node",
        parameters=[joy_config],
        remappings=[("/cmd_vel", LaunchConfiguration("cmd_vel_topic"))],
        output="both",
    )

    return LaunchDescription(declared_arguments + [joy_node, teleop_node])
