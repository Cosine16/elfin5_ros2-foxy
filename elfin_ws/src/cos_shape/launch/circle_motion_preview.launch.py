#!/usr/bin/env python3
# 启动 cos_shape 圆周运动预览节点（先规划显示，用户确认后执行）
import os

import xacro
import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    xacro_file = os.path.join(
        get_package_share_directory('elfin5_ros2_gazebo'),
        'urdf', 'elfin5.urdf.xacro')
    robot_description = {
        'robot_description': xacro.process_file(
            xacro_file,
            mappings={'use_fake_hardware': 'false', 'use_real_hardware': 'false'}
        ).toxml()
    }

    srdf_file = os.path.join(
        get_package_share_directory('elfin5_ros2_moveit2'),
        'config', 'elfin5.srdf')
    with open(srdf_file, 'r') as f:
        robot_description_semantic = {'robot_description_semantic': f.read()}

    kinematics_file = os.path.join(
        get_package_share_directory('elfin5_ros2_moveit2'),
        'config', 'kinematics.yaml')
    with open(kinematics_file, 'r') as f:
        robot_description_kinematics = {'robot_description_kinematics': yaml.safe_load(f)}

    return LaunchDescription([
        Node(
            package='cos_shape',
            executable='circle_motion_preview',
            name='circle_motion_preview',
            output='screen',
            parameters=[
                robot_description,
                robot_description_semantic,
                robot_description_kinematics,
                {
                    'group_name': 'elfin_arm',
                    'ee_link': 'elfin_end_link',
                    'center': [0.3, 0.0, 0.3],
                    'radius': 0.05,
                    'angular_velocity': 0.5,
                    'linear_velocity': 0.05,
                    'speed_mode': 'angular',
                    'inclination': 0.0,
                    'azimuth': 0.0,
                    'waypoints_per_rev': 64,
                    'revolutions': -1,
                    'eef_step': 0.005,
                    'autostart': False,
                },
            ],
        ),
    ])
