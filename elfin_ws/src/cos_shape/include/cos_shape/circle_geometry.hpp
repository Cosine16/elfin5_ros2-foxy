#ifndef COS_SHAPE__CIRCLE_GEOMETRY_HPP_
#define COS_SHAPE__CIRCLE_GEOMETRY_HPP_

#include <array>
#include <vector>

#include <geometry_msgs/msg/pose.hpp>

namespace cos_shape
{
namespace geometry
{

using Vec3 = std::array<double, 3>;

struct CircleDefinition
{
  Vec3 center;
  double radius;
  double inclination;
  double azimuth;
};

Vec3 circlePoint(const CircleDefinition& circle, double theta);

std::vector<geometry_msgs::msg::Pose> makeCircleWaypoints(
    const CircleDefinition& circle,
    const geometry_msgs::msg::Quaternion& orientation,
    int waypoint_count);

}  // namespace geometry
}  // namespace cos_shape

#endif  // COS_SHAPE__CIRCLE_GEOMETRY_HPP_
