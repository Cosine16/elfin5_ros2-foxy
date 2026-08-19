# 切换到工作路径，并加载环境变量
cd /home/fit/cos_ws/ros2_ws
source ./install/setup.bash
# usage: ros2 launch [-h] [-n] [-d] [-p | -s] [-a]
#                   package_name [launch_file_name] [launch_arguments [launch_arguments ...]]

# 启动elfin5 moveit2
ros2 launch elfin5_ros2_moveit2 elfin5.launch.py
# 启动gazebo仿真
ros2 launch elfin_basic_api fake_elfin_gui.launch.py