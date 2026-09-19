#include <cos_shape/trajectory_retimer.hpp>

#include <cstdint>

#include <moveit/robot_trajectory/robot_trajectory.h>
#include <moveit/trajectory_processing/iterative_time_parameterization.h>

namespace cos_shape
{
namespace trajectory
{
namespace
{
void scaleTrajectoryTime(trajectory_msgs::msg::JointTrajectory& joint_trajectory, double scale)
{
  for (auto& point : joint_trajectory.points)
  {
    const int64_t nanoseconds = static_cast<int64_t>(point.time_from_start.sec) * 1000000000LL +
                                point.time_from_start.nanosec;
    const int64_t scaled_nanoseconds = static_cast<int64_t>(nanoseconds * scale);
    point.time_from_start.sec = static_cast<int32_t>(scaled_nanoseconds / 1000000000LL);
    point.time_from_start.nanosec = static_cast<uint32_t>(scaled_nanoseconds % 1000000000LL);
    for (auto& velocity : point.velocities)
      velocity /= scale;
    for (auto& acceleration : point.accelerations)
      acceleration /= (scale * scale);
  }
}
}  // namespace

bool retimeToPeriod(
    moveit::planning_interface::MoveGroupInterface& move_group,
    const std::string& group_name,
    double target_period,
  moveit_msgs::msg::RobotTrajectory& trajectory,
  double* original_period_output)
{
  if (target_period <= 1e-9)
    return false;

  const auto state = move_group.getCurrentState(2.0);
  if (!state)
    return false;

  robot_trajectory::RobotTrajectory robot_trajectory(move_group.getRobotModel(), group_name);
  robot_trajectory.setRobotTrajectoryMsg(*state, trajectory);

  trajectory_processing::IterativeParabolicTimeParameterization parameterization;
  if (!parameterization.computeTimeStamps(robot_trajectory, 1.0, 1.0))
    return false;
  robot_trajectory.getRobotTrajectoryMsg(trajectory);

  auto& points = trajectory.joint_trajectory.points;
  if (points.empty())
    return false;

  const auto& last = points.back().time_from_start;
  const double original_period = static_cast<double>(last.sec) +
                                 static_cast<double>(last.nanosec) * 1e-9;
  if (original_period <= 1e-9)
    return false;

  if (original_period_output != nullptr)
    *original_period_output = original_period;

  scaleTrajectoryTime(trajectory.joint_trajectory, target_period / original_period);
  return true;
}

}  // namespace trajectory
}  // namespace cos_shape
