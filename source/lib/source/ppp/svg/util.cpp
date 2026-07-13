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
                          bool mirror_vertical,
                          bool mirror_horizontal,
                          Rotation rotation,
                          Size pixel_scaling)
{
    const auto rot_size{
        [&]()
        {
            switch (rotation)
            {
            case Rotation::None:
            case Rotation::Degree180:
                return svg.m_Size;
            case Rotation::Degree90:
            case Rotation::Degree270:
                return dla::rotl(svg.m_Size);
            }
            std::unreachable();
        }()
    };
    const auto scale{ size / rot_size };
    auto to_qpoint{
        [&](auto pt)
        {
            if (mirror_vertical)
            {
                pt.y = svg.m_Size.y - pt.y;
            }
            if (mirror_horizontal)
            {
                pt.x = svg.m_Size.x - pt.x;
            }

            const auto relative{
                [&]()
                {
                    return pt - svg.m_Size / 2;
                }
            };

            const auto absolute{
                [&](const auto& rel)
                {
                    return rel + rot_size / 2;
                }
            };

            switch (rotation)
            {
            case Rotation::None:
                break;
            case Rotation::Degree90:
            {
                auto rel{ relative() };
                std::swap(rel.x, rel.y);
                rel.x *= -1;
                pt = absolute(rel);
                break;
            }
            case Rotation::Degree180:
            {
                auto rel{ relative() };
                rel.x *= -1;
                rel.y *= -1;
                pt = absolute(rel);
                break;
            }
            case Rotation::Degree270:
            {
                auto rel{ relative() };
                std::swap(rel.x, rel.y);
                rel.y *= -1;
                pt = absolute(rel);
                break;
            }
            }

            pt *= scale;
            pt += offset;
            const auto px{ pt / pixel_scaling };
            return QPointF{ px.x, px.y };
        }
    };
    auto move_to{
        [to_qpoint](QPainterPath& path,
                    const auto& pt)
        {
            path.moveTo(to_qpoint(pt));
        }
    };
    auto line_to{
        [to_qpoint](QPainterPath& path,
                    const auto& /*from*/,
                    const auto& to)
        {
            path.lineTo(to_qpoint(to.m_Position));
        }
    };
    auto cubic_to{
        [to_qpoint](QPainterPath& path,
                    const auto& from,
                    const auto& to)
        {
            path.cubicTo(to_qpoint(from.m_NextHandle),
                         to_qpoint(to.m_PrevHandle),
                         to_qpoint(to.m_Position));
        }
    };

    DrawSvgToPath(path, move_to, line_to, cubic_to, [](QPainterPath&) {}, svg);
}

QPainterPath ConvertSvgToPainterPath(const Svg& svg,
                                     Rotation rotation)
{
    const auto rot_size{
        [&]()
        {
            switch (rotation)
            {
            case Rotation::None:
            case Rotation::Degree180:
                return svg.m_Size;
            case Rotation::Degree90:
            case Rotation::Degree270:
                return dla::rotl(svg.m_Size);
            }
            std::unreachable();
        }()
    };

    QPainterPath path;
    DrawSvgToPainterPath(path,
                         svg,
                         Position{},
                         rot_size,
                         false,
                         false,
                         rotation,
                         Size{ 1_mm, 1_mm });
    return path;
}
