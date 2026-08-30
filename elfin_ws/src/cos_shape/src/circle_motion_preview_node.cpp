/**
 * @file circle_motion_preview_node.cpp
 * @brief 先规划并在 RViz 中显示路径，用户确认后再执行圆周运动。
 *
 * 设计目标：
 * 1. 先调用 MoveIt 计算一圈圆轨迹并发布到 /move_group/display_planned_path
 * 2. 让 RViz 中的 MotionPlanning 插件显示黄色规划结果
 * 3. 用户确认后，通过 ~/execute=true 触发真实执行
 */

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/point.hpp>
#include <geometry_msgs/msg/pose.hpp>
#include <moveit_msgs/msg/display_trajectory.hpp>
#include <moveit_msgs/msg/robot_trajectory.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/string.hpp>
#include <trajectory_msgs/msg/joint_trajectory.hpp>

#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/robot_state/conversions.h>
#include <moveit/robot_trajectory/robot_trajectory.h>
#include <moveit/trajectory_processing/iterative_time_parameterization.h>

namespace
{
constexpr double kTwoPi = 6.283185307179586;
using Vec3 = std::array<double, 3>;

Vec3 cross(const Vec3& a, const Vec3& b)
{
  return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}

double norm(const Vec3& v)
{
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

Vec3 normalize(const Vec3& v)
{
  const double n = norm(v);
  return { v[0] / n, v[1] / n, v[2] / n };
}

void scaleTrajectoryTime(trajectory_msgs::msg::JointTrajectory& jt, double k)
{
  for (auto& p : jt.points)
  {
    const int64_t ns = static_cast<int64_t>(p.time_from_start.sec) * 1000000000LL + p.time_from_start.nanosec;
    const int64_t scaled = static_cast<int64_t>(ns * k);
    p.time_from_start.sec = static_cast<int32_t>(scaled / 1000000000LL);
    p.time_from_start.nanosec = static_cast<uint32_t>(scaled % 1000000000LL);
    for (auto& v : p.velocities)
      v /= k;
    for (auto& a : p.accelerations)
      a /= (k * k);
  }
}
}  // namespace

class CircleMotionPreview : public rclcpp::Node
{
public:
  struct Config
  {
    std::vector<double> center;
    double radius;
    double angular_velocity;
    double linear_velocity;
    std::string speed_mode;
    double inclination;
    double azimuth;
  };

  CircleMotionPreview() : Node("circle_motion_preview")
  {
    group_name_ = declare_parameter<std::string>("group_name", "elfin_arm");
    ee_link_ = declare_parameter<std::string>("ee_link", "elfin_end_link");
    cfg_.center = declare_parameter<std::vector<double>>("center", { 0.3, 0.0, 0.3 });
    cfg_.radius = declare_parameter<double>("radius", 0.05);
    cfg_.angular_velocity = declare_parameter<double>("angular_velocity", 0.5);
    cfg_.linear_velocity = declare_parameter<double>("linear_velocity", 0.05);
    cfg_.speed_mode = declare_parameter<std::string>("speed_mode", "angular");
    cfg_.inclination = declare_parameter<double>("inclination", 0.0);
    cfg_.azimuth = declare_parameter<double>("azimuth", 0.0);
    waypoints_per_rev_ = declare_parameter<int>("waypoints_per_rev", 64);
    revolutions_ = declare_parameter<int>("revolutions", -1);
    eef_step_ = declare_parameter<double>("eef_step", 0.005);
    autostart_ = declare_parameter<bool>("autostart", false);

    if (cfg_.center.size() != 3)
    {
      RCLCPP_WARN(get_logger(), "center 参数必须为 3 个值，回退为 [0.3, 0.0, 0.3]");
      cfg_.center = { 0.3, 0.0, 0.3 };
    }

    sub_angular_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_angular_velocity", 10, [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.angular_velocity = msg->data;
          cfg_.speed_mode = "angular";
        });
    sub_linear_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_linear_velocity", 10, [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.linear_velocity = msg->data;
          cfg_.speed_mode = "linear";
        });
    sub_center_ = create_subscription<geometry_msgs::msg::Point>(
        "~/set_center", 10, [this](const geometry_msgs::msg::Point::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.center = { msg->x, msg->y, msg->z };
        });
    sub_inclination_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_inclination", 10, [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.inclination = msg->data;
        });
    sub_azimuth_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_azimuth", 10, [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.azimuth = msg->data;
        });
    sub_radius_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_radius", 10, [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.radius = msg->data;
        });
    sub_enable_ = create_subscription<std_msgs::msg::Bool>(
        "~/enable", 10, [this](const std_msgs::msg::Bool::SharedPtr msg) {
          enabled_.store(msg->data);
          if (msg->data)
            RCLCPP_INFO(get_logger(), "收到启动命令，开始规划预览");
          else
            RCLCPP_INFO(get_logger(), "收到停止命令");
        });
    sub_execute_ = create_subscription<std_msgs::msg::Bool>(
        "~/execute", 10, [this](const std_msgs::msg::Bool::SharedPtr msg) {
          if (msg->data)
            execute_requested_.store(true);
        });

    pub_state_ = create_publisher<std_msgs::msg::String>("~/state", 10);
    pub_display_ = create_publisher<moveit_msgs::msg::DisplayTrajectory>("/move_group/display_planned_path", 10);

    enabled_.store(autostart_);
  }

  void run()
  {
    moveit::planning_interface::MoveGroupInterface mgi(shared_from_this(), group_name_);
    mgi.setMaxVelocityScalingFactor(0.3);
    mgi.setMaxAccelerationScalingFactor(0.3);

    RCLCPP_INFO(get_logger(), "规划坐标系: %s, 末端连杆: %s", mgi.getPlanningFrame().c_str(), ee_link_.c_str());
    RCLCPP_INFO(get_logger(), "等待 ~/enable=true，并在 RViz 中看到规划预览后，再发送 ~/execute=true 执行");

    while (rclcpp::ok())
    {
      if (!enabled_.load())
      {
        preview_ready_.store(false);
        execute_requested_.store(false);
        rclcpp::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      Config cfg = snapshot();
      if (cfg.radius <= 1e-6)
      {
        RCLCPP_ERROR(get_logger(), "radius 必须 > 0，停止。");
        enabled_.store(false);
        continue;
      }

      auto current_pose = mgi.getCurrentPose(ee_link_);
      const geometry_msgs::msg::Quaternion keep_orientation = current_pose.pose.orientation;

      if (!preview_ready_.load())
      {
        geometry_msgs::msg::Pose start_pose;
        const Vec3 p0 = circlePoint(cfg, 0.0);
        start_pose.position.x = p0[0];
        start_pose.position.y = p0[1];
        start_pose.position.z = p0[2];
        start_pose.orientation = keep_orientation;
        mgi.setPoseTarget(start_pose, ee_link_);
        if (mgi.move() != moveit::planning_interface::MoveItErrorCode::SUCCESS)
        {
          RCLCPP_ERROR(get_logger(), "无法到达圆周起点，停止。");
          enabled_.store(false);
          continue;
        }

        moveit_msgs::msg::RobotTrajectory traj;
        const double fraction = planOneRevolution(mgi, cfg, keep_orientation, traj);
        if (fraction < 0.99)
        {
          RCLCPP_ERROR(get_logger(), "圆周路径仅覆盖 %.1f%%，路径不可达，停止。", fraction * 100.0);
          enabled_.store(false);
          continue;
        }

        if (!retimeToPeriod(mgi, cfg, traj))
        {
          RCLCPP_ERROR(get_logger(), "时间参数化失败，停止。");
          enabled_.store(false);
          continue;
        }

        preview_traj_ = traj;
        publishDisplayTrajectory(mgi, preview_traj_);
        preview_ready_.store(true);
        RCLCPP_INFO(get_logger(), "规划预览已发布到 RViz，等待 ~/execute=true 才正式执行一圈");
      }

      if (!execute_requested_.load())
      {
        rclcpp::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      execute_requested_.store(false);
      preview_ready_.store(false);

      auto code = mgi.execute(preview_traj_);
      if (code != moveit::planning_interface::MoveItErrorCode::SUCCESS)
      {
        RCLCPP_ERROR(get_logger(), "执行失败，返回码: %d", code.val);
        enabled_.store(false);
        continue;
      }

      ++rev_;
      publishState(cfg, rev_);

      if (revolutions_ >= 0 && rev_ >= revolutions_)
      {
        enabled_.store(false);
        RCLCPP_INFO(get_logger(), "已完成 %d 圈，等待下一次 enable。", rev_);
      }
    }
  }

private:
  Config snapshot()
  {
    std::lock_guard<std::mutex> lk(mtx_);
    return cfg_;
  }

  Vec3 circlePoint(const Config& cfg, double theta) const
  {
    const Vec3 n = { std::sin(cfg.inclination) * std::cos(cfg.azimuth),
                     std::sin(cfg.inclination) * std::sin(cfg.azimuth),
                     std::cos(cfg.inclination) };
    Vec3 ref = { 1.0, 0.0, 0.0 };
    if (std::fabs(n[0]) > 0.99)
      ref = { 0.0, 1.0, 0.0 };
    const double d = ref[0] * n[0] + ref[1] * n[1] + ref[2] * n[2];
    const Vec3 u = normalize({ ref[0] - d * n[0], ref[1] - d * n[1], ref[2] - d * n[2] });
    const Vec3 v = cross(n, u);
    return { cfg.center[0] + cfg.radius * (std::cos(theta) * u[0] + std::sin(theta) * v[0]),
             cfg.center[1] + cfg.radius * (std::cos(theta) * u[1] + std::sin(theta) * v[1]),
             cfg.center[2] + cfg.radius * (std::cos(theta) * u[2] + std::sin(theta) * v[2]) };
  }

  double planOneRevolution(
      moveit::planning_interface::MoveGroupInterface& mgi, const Config& cfg,
      const geometry_msgs::msg::Quaternion& orientation,
      moveit_msgs::msg::RobotTrajectory& traj)
  {
    std::vector<geometry_msgs::msg::Pose> waypoints;
    waypoints.reserve(waypoints_per_rev_);
    for (int i = 1; i <= waypoints_per_rev_; ++i)
    {
      const double theta = kTwoPi * i / static_cast<double>(waypoints_per_rev_);
      const Vec3 p = circlePoint(cfg, theta);
      geometry_msgs::msg::Pose pose;
      pose.position.x = p[0];
      pose.position.y = p[1];
      pose.position.z = p[2];
      pose.orientation = orientation;
      waypoints.push_back(pose);
    }
    return mgi.computeCartesianPath(waypoints, eef_step_, 0.0, traj);
  }

  bool retimeToPeriod(
      moveit::planning_interface::MoveGroupInterface& mgi, const Config& cfg,
      moveit_msgs::msg::RobotTrajectory& traj)
  {
    double period = 0.0;
    if (cfg.speed_mode == "linear")
    {
      if (cfg.linear_velocity <= 1e-9)
        return false;
      period = kTwoPi * cfg.radius / cfg.linear_velocity;
    }
    else
    {
      if (cfg.angular_velocity <= 1e-9)
        return false;
      period = kTwoPi / cfg.angular_velocity;
    }

    auto state = mgi.getCurrentState(2.0);
    if (!state)
      return false;

    robot_trajectory::RobotTrajectory rt(mgi.getRobotModel(), group_name_);
    rt.setRobotTrajectoryMsg(*state, traj);
    trajectory_processing::IterativeParabolicTimeParameterization iptp;
    if (!iptp.computeTimeStamps(rt, 1.0, 1.0))
      return false;
    rt.getRobotTrajectoryMsg(traj);

    auto& points = traj.joint_trajectory.points;
    if (points.empty())
      return false;

    const auto& last = points.back().time_from_start;
    const double t_old = static_cast<double>(last.sec) + static_cast<double>(last.nanosec) * 1e-9;
    if (t_old <= 1e-9)
      return false;

    const double k = period / t_old;
    if (k < 1.0)
    {
      RCLCPP_WARN(get_logger(),
                  "目标周期 %.2fs 快于机械臂满速能力 %.2fs，将被关节速度限制放慢。",
                  period, t_old);
    }
    scaleTrajectoryTime(traj.joint_trajectory, k);
    return true;
  }

  void publishDisplayTrajectory(
      moveit::planning_interface::MoveGroupInterface& mgi,
      const moveit_msgs::msg::RobotTrajectory& traj)
  {
    moveit_msgs::msg::DisplayTrajectory display_msg;
    display_msg.model_id = mgi.getRobotModel()->getName();

    auto state = mgi.getCurrentState(2.0);
    if (state)
    {
      moveit::core::robotStateToRobotStateMsg(*state, display_msg.trajectory_start);
    }

    display_msg.trajectory.push_back(traj);
    pub_display_->publish(display_msg);
    RCLCPP_INFO(get_logger(), "已发布 RViz 规划预览，主题: /move_group/display_planned_path");
  }

  void publishState(const Config& cfg, int rev)
  {
    std_msgs::msg::String msg;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "rev=%d center=[%.3f,%.3f,%.3f] radius=%.3f mode=%s omega=%.3f v=%.3f incl=%.3f",
                  rev, cfg.center[0], cfg.center[1], cfg.center[2], cfg.radius, cfg.speed_mode.c_str(),
                  cfg.angular_velocity, cfg.linear_velocity, cfg.inclination);
    msg.data = buf;
    pub_state_->publish(msg);
  }

  std::string group_name_;
  std::string ee_link_;
  int waypoints_per_rev_;
  int revolutions_;
  double eef_step_;
  bool autostart_;

  Config cfg_;
  mutable std::mutex mtx_;
  std::atomic<bool> enabled_{false};
  std::atomic<bool> preview_ready_{false};
  std::atomic<bool> execute_requested_{false};
  int rev_{0};
  moveit_msgs::msg::RobotTrajectory preview_traj_;

  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_angular_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_linear_;
  rclcpp::Subscription<geometry_msgs::msg::Point>::SharedPtr sub_center_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_inclination_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_azimuth_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_radius_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr sub_enable_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr sub_execute_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_state_;
  rclcpp::Publisher<moveit_msgs::msg::DisplayTrajectory>::SharedPtr pub_display_;
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<CircleMotionPreview>();

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  std::thread spinner([&executor]() { executor.spin(); });

  node->run();

  rclcpp::shutdown();
  spinner.join();
  return 0;
}
