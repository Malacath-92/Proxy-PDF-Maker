#pragma once

#include <vector>

#include <ppp/util.hpp>

struct CubicBezierStartingControlPoint
{
    Position m_Position;
    Position m_NextHandle;
};
struct CubicBezierControlPoint
{
    Position m_PrevHandle;
    Position m_Position;
    Position m_NextHandle;
};
struct CubicBezierEndingControlPoint
{
    Position m_PrevHandle;
    Position m_Position;
};

struct CubicBezier
{
    CubicBezierStartingControlPoint m_StartPoint;
    std::vector<CubicBezierControlPoint> m_ControlPoints;
    CubicBezierEndingControlPoint m_EndPoint;
};

struct Svg
{
    // Size is only the precomputed AABB of the bezier
    Size m_Size;
    std::vector<CubicBezier> m_Curves;
};

Svg LoadSvg(const fs::path& svg_path);

template<class PathT,
         class MoveToT,
         class LineToT,
         class CubicBezierToT,
         class EndPathT>
void DrawSvgToPath(PathT& path,
                   MoveToT move_to,
                   LineToT line_to,
                   CubicBezierToT cubic_to,
                   EndPathT end_path,
                   const Svg& svg);

class QPainterPath;
void DrawSvgToPainterPath(QPainterPath& path,
                          const Svg& svg,
                          Position offset,
                          Size size,
                          bool mirror_vertical,
                          bool mirror_horizontal,
                          Rotation rotation,
                          Size pixel_scaling);
QPainterPath ConvertSvgToPainterPath(const Svg& svg,
                                     Rotation rotation = Rotation::None);

template<class PathT,
         class MoveToT,
         class LineToT,
         class CubicBezierToT,
         class EndPathT>
void DrawSvgToPath(PathT& path,
                   MoveToT move_to,
                   LineToT line_to,
                   CubicBezierToT cubic_to,
                   EndPathT end_path,
                   const Svg& svg)
{
    const auto curve_to{
        [&](PathT& path,
            const auto& from,
            const auto& to)
        {
            const bool is_linear{
                (from.m_NextHandle.x == to.m_PrevHandle.x && from.m_NextHandle.x == to.m_Position.x) ||
                (from.m_NextHandle.y == to.m_PrevHandle.y && from.m_NextHandle.y == to.m_Position.y)
            };
            if (is_linear)
            {
                line_to(path, from, to);
            }
            else
            {
                cubic_to(path, from, to);
            }
        }
    };

    for (const auto& curve : svg.m_Curves)
    {
        // Move to start of curve ...
        move_to(path, curve.m_StartPoint.m_Position);

        if (curve.m_ControlPoints.empty())
        {
            // ... draw curve to end point if the curve has only two nodes ...
            curve_to(path, curve.m_StartPoint, curve.m_EndPoint);
        }
        else
        {
            // ... or draw curve to first point ...
            curve_to(path, curve.m_StartPoint, curve.m_ControlPoints.front());

            for (size_t i{ 0 }; i < curve.m_ControlPoints.size() - 1; i++)
            {
                const size_t j{ i + 1 };
                // ... continue drawing curves to next point ...
                curve_to(path, curve.m_ControlPoints[i], curve.m_ControlPoints[j]);
            }

            // ... until we draw curve to the last point ...
            curve_to(path, curve.m_ControlPoints.back(), curve.m_EndPoint);
        }

        // ... and finally finish up the path.
        end_path(path);
    }
}
