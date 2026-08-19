cd ~/cos_ws/references
source ./install/setup.bash
sudo chrt 10 bash
# ↓ 进入 root 实时 shell 后,再 source 一次(环境不会继承)
source ~/cos_ws/references/install/setup.bash
ros2 launch elfin5_ros2_moveit2 elfin5_moveit.launch.py

cd ~/cos_ws/references
source ./install/setup.bash
sudo -E bash -c "source ~/cos_ws/references/install/setup.bash && ros2 launch elfin5_ros2_moveit2 elfin5_moveit_rviz.launch.py"

cd ~/cos_ws/references
source ./install/setup.bash
sudo -E bash -c "source ~/cos_ws/references/install/setup.bash && ros2 launch elfin5_ros2_moveit2 elfin5_basic_api.launch.py"

cd ~/cos_ws/references
source ./install/setup.bash
sudo -E bash -c "source ~/cos_ws/references/install/setup.bash && ros2 launch elfin_basic_api elfin_gui.launch.py"

