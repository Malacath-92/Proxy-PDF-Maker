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
    Size m_Size;
    std::vector<CubicBezier> m_Curves;
};

Svg LoadSvg(const fs::path& svg_path);

class QPainterPath;
void DrawSvgToPainterPath(QPainterPath& path,
                          const Svg& svg,
                          Position offset,
                          Size size,
                          Size pixel_scaling);
QPainterPath ConvertSvgToPainterPath(const Svg& svg);
