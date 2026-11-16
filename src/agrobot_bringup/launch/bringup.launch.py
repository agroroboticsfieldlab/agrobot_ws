# Brings up an agrobot (robot:=curio, default, or robot:=atd): ros2_control with the ODrive hardware interface, robot_state_publisher,
# joint_state_broadcaster, the diff drive controller and (optionally) F710 gamepad teleop.
#
# Commands: geometry_msgs/msg/TwistStamped on /agrobot/cmd_vel
# Odometry: /agrobot/odom
# Robots:   curio = 2 driven wheels + 2 casters, atd = 4-wheel skid steering (agrobot_description)
# Teleop:   hold LB to drive, LB + RB for turbo (see teleop_f710.launch.py). Disable with use_teleop:=false.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, RegisterEventHandler
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, FindExecutable, LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    declared_arguments = [
        DeclareLaunchArgument(
            "robot",
            default_value="curio",
            choices=["curio", "atd"],
            description="Which agrobot to bring up: curio (2 wheels + casters) or atd (4-wheel skid steering).",
        ),
        DeclareLaunchArgument(
            "use_mock_hardware",
            default_value="false",
            description="Use mock hardware instead of the ODrives (nothing moves).",
        ),
        DeclareLaunchArgument(
            "use_teleop",
            default_value="true",
            description="Start joy_node and teleop_twist_joy for the Logitech F710 gamepad.",
        ),
    ]
    use_mock_hardware = LaunchConfiguration("use_mock_hardware")
    robot = LaunchConfiguration("robot")

    description_pkg = FindPackageShare("agrobot_description")

    robot_description_content = Command(
        [
            PathJoinSubstitution([FindExecutable(name="xacro")]),
            " ",
            PathJoinSubstitution([description_pkg, "urdf", ["agrobot_", robot, ".urdf.xacro"]]),
            " use_mock_hardware:=",
            use_mock_hardware,
        ]
    )
    robot_description = {"robot_description": ParameterValue(robot_description_content, value_type=str)}

    robot_controllers = PathJoinSubstitution(
        [FindPackageShare("agrobot_bringup"), "config", ["agrobot_", robot, "_controllers.yaml"]]
    )

    control_node = Node(
        package="controller_manager",
        executable="ros2_control_node",
        parameters=[robot_description, robot_controllers],
        output="both",
    )

    robot_state_pub_node = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        parameters=[robot_description],
        output="both",
    )

    joint_state_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster", "--controller-manager", "/controller_manager"],
    )

    robot_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["agrobot", "--controller-manager", "/controller_manager"],
    )

    # Start the diff drive controller only after joint_state_broadcaster is up
    delay_robot_controller_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=joint_state_broadcaster_spawner,
            on_exit=[robot_controller_spawner],
        )
    )

    teleop = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([FindPackageShare("agrobot_bringup"), "launch", "teleop_f710.launch.py"])
        ),
        launch_arguments={"cmd_vel_topic": "/agrobot/cmd_vel"}.items(),
        condition=IfCondition(LaunchConfiguration("use_teleop")),
    )

    return LaunchDescription(
        declared_arguments
        + [
            control_node,
            robot_state_pub_node,
            joint_state_broadcaster_spawner,
            delay_robot_controller_spawner,
            teleop,
        ]
    )
