#include <ppp/svg/util.hpp>

#include <QPainterPath>

#include <nanosvg.h>

#include <ppp/util/log.hpp>

Svg LoadSvg(const fs::path& svg_path)
{
    const auto import_dpi{ 96 };
    auto* image{ nsvgParseFromFile(svg_path.string().c_str(), "mm", import_dpi) };
    AtScopeExit delete_image{ std::bind_front(nsvgDelete, image) };

    const auto width{ 1_in * image->width / import_dpi };
    const auto height{ 1_in * image->height / import_dpi };

    Svg output_svg{
        .m_Size{ width, height },
        .m_Curves{},
    };
    for (NSVGshape* shape{ image->shapes }; shape != nullptr; shape = shape->next)
    {
        for (NSVGpath* path{ shape->paths }; path != nullptr; path = path->next)
        {
            auto& curve{ output_svg.m_Curves.emplace_back() };
            static_assert(sizeof(dla::vec2) == sizeof(float) * 2);

            std::span points{
                reinterpret_cast<const dla::vec2*>(path->pts),
                static_cast<size_t>(path->npts)
            };

            curve.m_StartPoint = {
                .m_Position{ points[0] * 1_mm },
                .m_NextHandle{ points[1] * 1_mm },
            };

            for (size_t i{ 2 }; i < points.size(); i += 3)
            {
                if (i + 2 < points.size())
                {
                    curve.m_ControlPoints.push_back(
                        {
                            .m_PrevHandle{ points[i] * 1_mm },
                            .m_Position{ points[i + 1] * 1_mm },
                            .m_NextHandle{ points[i + 2] * 1_mm },
                        });
                }
                else if (i + 1 < points.size())
                {
                    curve.m_EndPoint = {
                        .m_PrevHandle{ points[i] * 1_mm },
                        .m_Position{ points[i + 1] * 1_mm },
                    };
                }
                else
                {
                    LogError("Error parsing SVG file: Bad bezier curve.m_");
                    return Svg{};
                }
            }
        }
    }

    return output_svg;
}

void DrawSvgToPainterPath(QPainterPath& path,
                          const Svg& svg,
                          Position offset,
                          Size size,
                          Size pixel_scaling)
{
    const auto scale{ size / svg.m_Size };
    auto to_qpoint{
        [&](auto pt)
        {
            pt *= scale;
            pt += offset;
            const auto px{ pt / pixel_scaling };
            return QPointF{ px.x, px.y };
        }
    };

    for (const auto& curve : svg.m_Curves)
    {
        // Move to start of curve ...
        path.moveTo(to_qpoint(curve.m_StartPoint.m_Position));

        if (curve.m_ControlPoints.empty())
        {
            // ... draw bezier to end point if the curve has only two nodes ...
            path.cubicTo(to_qpoint(curve.m_StartPoint.m_NextHandle),
                         to_qpoint(curve.m_EndPoint.m_PrevHandle),
                         to_qpoint(curve.m_EndPoint.m_Position));
        }
        else
        {
            // ... or draw bezier to first point ...
            path.cubicTo(to_qpoint(curve.m_StartPoint.m_NextHandle),
                         to_qpoint(curve.m_ControlPoints[0].m_PrevHandle),
                         to_qpoint(curve.m_ControlPoints[0].m_Position));

            for (size_t i{ 0 }; i < curve.m_ControlPoints.size(); i++)
            {
                size_t j{ i + 1 };
                if (j < curve.m_ControlPoints.size())
                {
                    // ... continue drawing beziers to next point ...
                    path.cubicTo(to_qpoint(curve.m_ControlPoints[i].m_NextHandle),
                                 to_qpoint(curve.m_ControlPoints[j].m_PrevHandle),
                                 to_qpoint(curve.m_ControlPoints[j].m_Position));
                }
                else
                {
                    // ... until we draw bezier to the last point
                    path.cubicTo(to_qpoint(curve.m_ControlPoints[i].m_NextHandle),
                                 to_qpoint(curve.m_EndPoint.m_PrevHandle),
                                 to_qpoint(curve.m_EndPoint.m_Position));
                }
            }
        }
    }
}

QPainterPath ConvertSvgToPainterPath(const Svg& svg)
{
    QPainterPath path;
    DrawSvgToPainterPath(path, svg, Position{}, svg.m_Size, Size{ 1_mm, 1_mm });
    return path;
}
