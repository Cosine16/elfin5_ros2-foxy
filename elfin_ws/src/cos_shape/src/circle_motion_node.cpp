/**
 * @file circle_motion_node.cpp
 * @brief 控制 Elfin5 机械臂末端沿指定三维圆轨迹运动。
 *
 * 圆轨迹由圆心、半径、平面倾角和方位角确定。节点首先使用
 * MoveIt Cartesian Path 生成一圈的笛卡尔路径，再进行时间参数化，
 * 从而支持角速度模式和线速度模式。
 *
 * 末端姿态在运动期间保持开始执行时的姿态不变。运行期间通过 Topic
 * 修改的圆轨迹参数在下一圈规划时生效。
 *
 * @par Parameters
 * - `group_name`：MoveIt 规划组名称。
 * - `ee_link`：末端执行器连杆名称。
 * - `center`：规划坐标系中的圆心 `[x, y, z]`，单位为 m。
 * - `radius`：圆半径，单位为 m。
 * - `angular_velocity`：角速度，单位为 rad/s。
 * - `linear_velocity`：线速度，单位为 m/s。
 * - `speed_mode`：速度模式，取 `angular` 或 `linear`。
 * - `inclination`：圆平面法线相对基座 Z 轴的倾角，单位为 rad。
 * - `azimuth`：圆平面法线绕基座 Z 轴的方位角，单位为 rad。
 * - `waypoints_per_rev`：每圈笛卡尔路径点数量。
 * - `revolutions`：执行圈数，`-1` 表示无限执行。
 * - `eef_step`：笛卡尔路径插值步长，单位为 m。
 * - `autostart`：是否在节点启动后立即执行。
 *
 * @par ROS interfaces
 * 所有相对名称均位于节点私有命名空间 `~/` 下：
 * - `~/set_angular_velocity` (`std_msgs/msg/Float64`)：设置角速度并切换模式。
 * - `~/set_linear_velocity` (`std_msgs/msg/Float64`)：设置线速度并切换模式。
 * - `~/set_center` (`geometry_msgs/msg/Point`)：设置圆心。
 * - `~/set_inclination` (`std_msgs/msg/Float64`)：设置平面倾角。
 * - `~/set_azimuth` (`std_msgs/msg/Float64`)：设置平面方位角。
 * - `~/set_radius` (`std_msgs/msg/Float64`)：设置圆半径。
 * - `~/enable` (`std_msgs/msg/Bool`)：`true` 开始，`false` 停止。
 * - `~/state` (`std_msgs/msg/String`)：每圈结束发布一次状态摘要。
 */

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/point.hpp>
#include <geometry_msgs/msg/pose.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/string.hpp>
#include <trajectory_msgs/msg/joint_trajectory.hpp>

#include <cos_shape/circle_geometry.hpp>
#include <cos_shape/trajectory_retimer.hpp>
#include <moveit/move_group_interface/move_group_interface.h>

namespace
{
constexpr double kTwoPi = 6.283185307179586;

}  // namespace

/**
 * @brief Elfin5 圆周运动 ROS 2 节点。
 *
 * 节点回调只更新受互斥锁保护的运行参数；实际规划和执行在
 * `run()` 中进行，以避免多个回调线程同时访问 MoveGroupInterface。
 */
class CircleMotion : public rclcpp::Node
{
public:
  /**
   * @brief 创建节点、声明参数并初始化 ROS Topic 接口。
   */
  CircleMotion() : Node("circle_motion")
  {
    // ---- 参数（可被 launch / --ros-args -p 覆盖）----
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
    revolutions_ = declare_parameter<int>("revolutions", -1);  // -1 = 无限
    eef_step_ = declare_parameter<double>("eef_step", 0.005);
    autostart_ = declare_parameter<bool>("autostart", false);
    if (cfg_.center.size() != 3)
    {
      RCLCPP_WARN(get_logger(), "center 参数必须是 3 个数字，回退到默认值 [0.3, 0, 0.3]");
      cfg_.center = { 0.3, 0.0, 0.3 };
    }

    // ---- Topic 接口 ----
    sub_angular_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_angular_velocity", 10,
        [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.angular_velocity = msg->data;
          cfg_.speed_mode = "angular";
          RCLCPP_INFO(get_logger(), "角速度更新为 %.4f rad/s（下一圈生效）", msg->data);
        });
    sub_linear_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_linear_velocity", 10,
        [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.linear_velocity = msg->data;
          cfg_.speed_mode = "linear";
          RCLCPP_INFO(get_logger(), "线速度更新为 %.4f m/s（下一圈生效）", msg->data);
        });
    sub_center_ = create_subscription<geometry_msgs::msg::Point>(
        "~/set_center", 10,
        [this](const geometry_msgs::msg::Point::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.center = { msg->x, msg->y, msg->z };
          RCLCPP_INFO(get_logger(), "圆心更新为 [%.3f, %.3f, %.3f]（下一圈生效）", msg->x, msg->y, msg->z);
        });
    sub_inclination_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_inclination", 10,
        [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.inclination = msg->data;
          RCLCPP_INFO(get_logger(), "倾角更新为 %.4f rad（下一圈生效）", msg->data);
        });
    sub_azimuth_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_azimuth", 10,
        [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.azimuth = msg->data;
          RCLCPP_INFO(get_logger(), "方位角更新为 %.4f rad（下一圈生效）", msg->data);
        });
    sub_radius_ = create_subscription<std_msgs::msg::Float64>(
        "~/set_radius", 10,
        [this](const std_msgs::msg::Float64::SharedPtr msg) {
          std::lock_guard<std::mutex> lk(mtx_);
          cfg_.radius = msg->data;
          RCLCPP_INFO(get_logger(), "半径更新为 %.4f m（下一圈生效）", msg->data);
        });
    sub_enable_ = create_subscription<std_msgs::msg::Bool>(
        "~/enable", 10,
        [this](const std_msgs::msg::Bool::SharedPtr msg) {
          enabled_.store(msg->data);
          RCLCPP_INFO(get_logger(), "%s", msg->data ? "收到启动指令" : "收到停止指令");
        });
    pub_state_ = create_publisher<std_msgs::msg::String>("~/state", 10);

    enabled_.store(autostart_);
  }

  /**
   * @brief 初始化 MoveIt 接口并执行圆周运动主循环。
   *
   * 该函数会阻塞，直到 ROS 关闭或节点完成/停止当前运动任务。
   */
  void run()
  {
    moveit::planning_interface::MoveGroupInterface mgi(shared_from_this(), group_name_);
    mgi.setMaxVelocityScalingFactor(0.3);
    mgi.setMaxAccelerationScalingFactor(0.3);
    const std::string planning_frame = mgi.getPlanningFrame();
    RCLCPP_INFO(get_logger(), "规划坐标系: %s, 末端连杆: %s", planning_frame.c_str(), ee_link_.c_str());
    RCLCPP_INFO(get_logger(), "等待 ~/enable = true ...");

    while (rclcpp::ok())
    {
      if (!enabled_.load())
      {
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

      // 固定末端姿态为启动时刻姿态
      auto current_pose = mgi.getCurrentPose(ee_link_);
      const geometry_msgs::msg::Quaternion keep_orientation = current_pose.pose.orientation;

      // 1) 先走到圆周起点 p(0)
      geometry_msgs::msg::Pose start_pose;
        const cos_shape::geometry::CircleDefinition circle{
          {cfg.center[0], cfg.center[1], cfg.center[2]}, cfg.radius, cfg.inclination, cfg.azimuth};
        const cos_shape::geometry::Vec3 p0 = cos_shape::geometry::circlePoint(circle, 0.0);
      start_pose.position.x = p0[0];
      start_pose.position.y = p0[1];
      start_pose.position.z = p0[2];
      start_pose.orientation = keep_orientation;
      mgi.setPoseTarget(start_pose, ee_link_);
      RCLCPP_INFO(get_logger(), "移动到圆周起点 [%.3f, %.3f, %.3f] ...", p0[0], p0[1], p0[2]);
      if (mgi.move() != moveit::planning_interface::MoveItErrorCode::SUCCESS)
      {
        RCLCPP_ERROR(get_logger(), "无法到达圆周起点（可能超出工作空间），停止。");
        enabled_.store(false);
        continue;
      }

      // 2) 逐圈执行
      int rev = 0;
      int consecutive_failures = 0;
      while (rclcpp::ok() && enabled_.load() && (revolutions_ < 0 || rev < revolutions_))
      {
        cfg = snapshot();
        moveit_msgs::msg::RobotTrajectory traj;
        const double fraction = planOneRevolution(mgi, cfg, keep_orientation, traj);
        if (fraction < 0.99)
        {
          RCLCPP_ERROR(get_logger(), "圆周路径仅覆盖 %.1f%%，轨迹不可达，停止。", fraction * 100.0);
          enabled_.store(false);
          break;
        }

        if (!retimeToPeriod(mgi, cfg, traj))
        {
          RCLCPP_ERROR(get_logger(), "时间参数化失败，停止。");
          enabled_.store(false);
          break;
        }

        // 异步执行，主循环轮询停止标志
        auto fut = std::async(std::launch::async, [&mgi, &traj]() { return mgi.execute(traj); });
        while (fut.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
        {
          if (!rclcpp::ok())
            return;
          if (!enabled_.load())
            mgi.stop();
        }
        const auto code = fut.get();
        if (code != moveit::planning_interface::MoveItErrorCode::SUCCESS)
        {
          if (!enabled_.load())
            break;  // 用户主动停止
          // 控制器侧偶发中止（如仿真时钟抖动导致执行超时）：跳过本圈，重新规划下一圈
          if (++consecutive_failures >= 5)
          {
            RCLCPP_ERROR(get_logger(), "连续 %d 圈执行失败，停止。", consecutive_failures);
            enabled_.store(false);
            break;
          }
          RCLCPP_WARN(get_logger(), "本圈执行失败（返回码 %d），跳过并重新规划下一圈（连续失败 %d/5）。",
                      code.val, consecutive_failures);
          continue;
        }

        consecutive_failures = 0;
        ++rev;
        publishState(cfg, rev);
      }
      enabled_.store(false);
      RCLCPP_INFO(get_logger(), "圆周运动结束（完成 %d 圈），等待下一次 enable。", rev);
    }
  }

private:
  struct Config
  {
    std::vector<double> center;       ///< 圆心坐标 `[x, y, z]`，单位为 m。
    double radius;                    ///< 圆半径，单位为 m。
    double angular_velocity;          ///< 角速度，单位为 rad/s。
    double linear_velocity;           ///< 线速度，单位为 m/s。
    std::string speed_mode;           ///< 速度模式：`angular` 或 `linear`。
    double inclination;               ///< 圆平面法线倾角，单位为 rad。
    double azimuth;                   ///< 圆平面法线方位角，单位为 rad。
  };

  /** @brief 获取当前运行参数的一致快照。 */
  Config snapshot()
  {
    std::lock_guard<std::mutex> lk(mtx_);
    return cfg_;
  }

  /**
   * @brief 规划一整圈保持姿态不变的笛卡尔路径。
   *
   * @param mgi MoveIt 规划组接口。
   * @param cfg 当前圆轨迹参数。
   * @param orientation 圆周运动期间保持不变的末端姿态。
   * @param traj 输出的关节轨迹。
   * @return 笛卡尔路径覆盖率，范围通常为 `[0, 1]`。
   */
  double planOneRevolution(
    moveit::planning_interface::MoveGroupInterface& mgi, const Config& cfg,
    const geometry_msgs::msg::Quaternion& orientation,
    moveit_msgs::msg::RobotTrajectory& traj)
  {
    const cos_shape::geometry::CircleDefinition circle{
        {cfg.center[0], cfg.center[1], cfg.center[2]}, cfg.radius, cfg.inclination, cfg.azimuth};
    const auto waypoints = cos_shape::geometry::makeCircleWaypoints(
        circle, orientation, waypoints_per_rev_);
    return mgi.computeCartesianPath(waypoints, eef_step_, 0.0 /*jump_threshold 禁用*/, traj);
  }

  /**
   * @brief 为轨迹添加时间戳，并缩放到目标圆周周期。
   *
   * 目标周期为角速度模式下的 $T = 2\pi / \omega$，或线速度模式下的
   * $T = 2\pi r / v$。最终速度仍受关节和控制器限制。
   *
   * @param mgi MoveIt 规划组接口。
   * @param cfg 当前圆轨迹参数。
   * @param traj 待重新时间参数化的轨迹，同时作为输出轨迹。
   * @return `true` 表示时间参数化成功，否则返回 `false`。
   */
  bool retimeToPeriod(
    moveit::planning_interface::MoveGroupInterface& mgi, const Config& cfg,
    moveit_msgs::msg::RobotTrajectory& traj)
  {
    // 目标周期
    double period;
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

    double original_period = 0.0;
    if (!cos_shape::trajectory::retimeToPeriod(
            mgi, group_name_, period, traj, &original_period))
      return false;

    if (period < original_period)
    {
      RCLCPP_WARN(get_logger(),
                  "目标周期 %.2fs 快于机械臂满速能力 %.2fs，将被关节速度限制放慢。"
                  "请降低速度。",
                  period, original_period);
    }
    RCLCPP_INFO(get_logger(), "本圈周期 %.2fs（模式 %s）", period, cfg.speed_mode.c_str());
    return true;
  }

  /**
   * @brief 发布一圈完成后的状态摘要。
   * @param cfg 本圈实际使用的圆轨迹参数。
   * @param rev 已完成的圈数。
   */
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

  // 参数
  std::string group_name_;
  std::string ee_link_;
  int waypoints_per_rev_;
  int revolutions_;
  double eef_step_;
  bool autostart_;

  Config cfg_;
  mutable std::mutex mtx_;
  std::atomic<bool> enabled_{ false };

  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_angular_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_linear_;
  rclcpp::Subscription<geometry_msgs::msg::Point>::SharedPtr sub_center_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_inclination_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_azimuth_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_radius_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr sub_enable_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_state_;
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<CircleMotion>();

  // MoveGroupInterface 依赖节点的回调（当前状态监听等），单独线程 spin
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  std::thread spinner([&executor]() { executor.spin(); });

  node->run();

  rclcpp::shutdown();
  spinner.join();
  return 0;
}
