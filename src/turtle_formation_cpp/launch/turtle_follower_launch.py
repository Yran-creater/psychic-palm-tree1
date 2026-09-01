from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import ExecuteProcess
from launch.actions import TimerAction

def generate_launch_description():
    # 1. 启动turtlesim仿真节点
    turtlesim_node = Node(
        package='turtlesim',
        executable='turtlesim_node',
        name='turtlesim_node',
        parameters=[
            {'background_r': 255, 'background_g': 255, 'background_b': 255},
            
        ],
        output='screen'
    )

    # 2. 启动你的编队控制节点
    formation_node = Node(
        package='turtle_formation_cpp',
        executable='formation_control_node',
        name='formation_control_node',
        output='screen'
    )

    spawn_turtle2 = ExecuteProcess(
    cmd=['ros2', 'service', 'call', '/spawn', 'turtlesim/srv/Spawn',
         '{x: 4.0, y: 5.54, theta: 0.0, name: "turtle2"}'],
    output='screen'
    )

    spawn_turtle3 = ExecuteProcess(
        cmd=['ros2', 'service', 'call', '/spawn', 'turtlesim/srv/Spawn',
         '{x: 2.5, y: 5.54, theta: 0.0, name: "turtle3"}'],
        output='screen'
    )

    # 5. 添加键盘控制节点（通过方向键手动控制turtle1）
    keyboard_teleop = Node(
    package='teleop_twist_keyboard',
    executable='teleop_twist_keyboard',
    name='keyboard_teleop',
    remappings=[('/cmd_vel', '/turtle1/cmd_vel')],
    prefix='xterm -e',
    output='screen'
)

    # 6. 添加2秒延迟，确保turtlesim完全启动后再生成海龟
    delayed_spawns = TimerAction(
        period=2.0,
        actions=[spawn_turtle2, spawn_turtle3]
    )

    return LaunchDescription([
        turtlesim_node,
        delayed_spawns,
        formation_node,
        keyboard_teleop
    ])
