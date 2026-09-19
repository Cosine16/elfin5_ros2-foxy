#ifndef COS_SHAPE__TRAJECTORY_RETIMER_HPP_
#define COS_SHAPE__TRAJECTORY_RETIMER_HPP_

#include <string>

#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit_msgs/msg/robot_trajectory.hpp>

namespace cos_shape
{
namespace trajectory
{

bool retimeToPeriod(
    moveit::planning_interface::MoveGroupInterface& move_group,
    const std::string& group_name,
    double target_period,
    moveit_msgs::msg::RobotTrajectory& trajectory,
    double* original_period = nullptr);

}  // namespace trajectory
}  // namespace cos_shape

#endif  // COS_SHAPE__TRAJECTORY_RETIMER_HPP_
