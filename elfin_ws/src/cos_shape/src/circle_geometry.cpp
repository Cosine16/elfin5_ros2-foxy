#include <cos_shape/circle_geometry.hpp>

#include <cmath>
#include <stdexcept>

namespace cos_shape
{
namespace geometry
{
namespace
{
constexpr double kTwoPi = 6.283185307179586;

Vec3 cross(const Vec3& a, const Vec3& b)
{
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double norm(const Vec3& value)
{
  return std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
}

Vec3 normalize(const Vec3& value)
{
  const double length = norm(value);
  if (length <= 1e-12)
    throw std::invalid_argument("circle plane basis is degenerate");
  return {value[0] / length, value[1] / length, value[2] / length};
}
}  // namespace

Vec3 circlePoint(const CircleDefinition& circle, double theta)
{
  const Vec3 normal = {
      std::sin(circle.inclination) * std::cos(circle.azimuth),
      std::sin(circle.inclination) * std::sin(circle.azimuth),
      std::cos(circle.inclination)};

  Vec3 reference = {1.0, 0.0, 0.0};
  if (std::fabs(normal[0]) > 0.99)
    reference = {0.0, 1.0, 0.0};

  const double projection = reference[0] * normal[0] + reference[1] * normal[1] + reference[2] * normal[2];
  const Vec3 u = normalize({
      reference[0] - projection * normal[0],
      reference[1] - projection * normal[1],
      reference[2] - projection * normal[2]});
  const Vec3 v = cross(normal, u);

  return {
      circle.center[0] + circle.radius * (std::cos(theta) * u[0] + std::sin(theta) * v[0]),
      circle.center[1] + circle.radius * (std::cos(theta) * u[1] + std::sin(theta) * v[1]),
      circle.center[2] + circle.radius * (std::cos(theta) * u[2] + std::sin(theta) * v[2])};
}

std::vector<geometry_msgs::msg::Pose> makeCircleWaypoints(
    const CircleDefinition& circle,
    const geometry_msgs::msg::Quaternion& orientation,
    int waypoint_count)
{
  if (waypoint_count <= 0)
    throw std::invalid_argument("waypoint_count must be positive");

  std::vector<geometry_msgs::msg::Pose> waypoints;
  waypoints.reserve(static_cast<size_t>(waypoint_count));
  for (int index = 1; index <= waypoint_count; ++index)
  {
    const Vec3 point = circlePoint(circle, kTwoPi * index / waypoint_count);
    geometry_msgs::msg::Pose pose;
    pose.position.x = point[0];
    pose.position.y = point[1];
    pose.position.z = point[2];
    pose.orientation = orientation;
    waypoints.push_back(pose);
  }
  return waypoints;
}

}  // namespace geometry
}  // namespace cos_shape
