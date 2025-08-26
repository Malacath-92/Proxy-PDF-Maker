#include <ppp/svg/generate.hpp>

#include <numbers>

#include <QPen>
#include <QRect>
#include <QSvgGenerator>

#include <ppp/qt_util.hpp>

#include <ppp/pdf/util.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

void DrawSvg(QPainter& painter, const QPainterPath& path, QColor color)
{
    painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);

    QPen pen{};
    pen.setWidth(1);
    pen.setColor(color);
    painter.setPen(pen);
    painter.drawPath(path);
}

QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 pixel_size,
                               const Project& project)
{
    QPainterPath card_border;
    if (project.m_Data.m_BleedEdge > 0_mm)
    {
        const QRectF rect{
            origin.x,
            origin.y,
            pixel_size.x,
            pixel_size.y,
        };
        card_border.addRect(rect);
    }

    const auto cards_size{ project.ComputeCardsSize() };
    const auto pixel_ratio{ pixel_size / cards_size };

    const auto transforms{ ComputeTransforms(project) };
    const auto bleed_edge{ project.m_Data.m_BleedEdge * pixel_ratio };
    const auto corner_radius{ project.CardCornerRadius() * pixel_ratio };
    const auto margins{ project.ComputeMargins() };

    const auto page_size{ project.ComputePageSize() };
    const Size available_space{
        page_size.x - margins.m_Left - margins.m_Right,
        page_size.y - margins.m_Top - margins.m_Bottom,
    };
    const Position cards_origin{
        margins.m_Left + (available_space.x - cards_size.x) / 2.0f,
        margins.m_Top + (available_space.y - cards_size.y) / 2.0f,
    };
    const auto cards_offset{ cards_origin * pixel_ratio };

    for (const auto& transform : transforms)
    {
        const auto top_left_corner{ transform.m_Position * pixel_ratio - cards_offset };
        const auto card_size{ transform.m_Size * pixel_ratio };

        const QRectF rect{
            origin.x + top_left_corner.x + bleed_edge.x,
            origin.y + top_left_corner.y + bleed_edge.y,
            card_size.x - bleed_edge.x * 2.0f,
            card_size.y - bleed_edge.x * 2.0f,
        };
        card_border.addRoundedRect(rect, corner_radius.x, corner_radius.y);
    }

    return card_border;
}

QPainterPath GenerateCardsPath(const Project& project)
{
    const auto svg_size{ project.ComputeCardsSize() / 1_mm };
    return GenerateCardsPath(dla::vec2{ 0.0f, 0.0f },
                             svg_size,
                             project);
}

void GenerateCardsSvg(const Project& project)
{
    const auto svg_path{ fs::path{ project.m_Data.m_FileName }.replace_extension(".svg") };

    LogInfo("Generating card path...");
    const QPainterPath path{ GenerateCardsPath(project) };

    QSvgGenerator generator{};
    generator.setFileName(ToQString(svg_path));
    generator.setTitle("Card cutting guides.");
    generator.setDescription("An SVG containing exact cutting guides for the accompanying sheet.");

    LogInfo("Drawing card path...");
    QPainter painter;
    painter.begin(&generator);
    DrawSvg(painter, path);
    painter.end();
}

void GenerateCardsDxf(const Project& project)
{
    const auto dxf_path{ fs::path{ project.m_Data.m_FileName }.replace_extension(".dxf") };

    LogInfo("Generating card path...");
    std::ofstream output{ dxf_path, std::ios_base::trunc };
    auto start_poly_line{
        [&]()
        {
            output << R"(  0
POLYLINE
  8
0
  66
1
  70
9
)";
            return AtScopeExit{
                [&output]()
                {
                    output << R"(  0
SEQEND
)";
                }
            };
        }
    };
    auto draw_vertex{
        [&](const dla::vec2 coord)
        {
            // clang-format off
            output << fmt::format(R"(  0
VERTEX
  8
0
  10
{}
  20
{}
)", coord.x, coord.y);
            // clang-format on
        }
    };

    output << R"(  0
SECTION
  2
ENTITIES
)";

    const auto cards_size{ project.ComputeCardsSize() / 1_mm };
    if (project.m_Data.m_BleedEdge > 0_mm)
    {
        auto poly_line{ start_poly_line() };

        draw_vertex(dla::vec2{ 0, 0 });
        draw_vertex(dla::vec2{ cards_size.x, 0 });
        draw_vertex(dla::vec2{ cards_size.x, cards_size.y });
        draw_vertex(dla::vec2{ 0, cards_size.y });
    }

    const auto transforms{ ComputeTransforms(project) };
    const auto bleed_edge{ project.m_Data.m_BleedEdge / 1_mm };
    const auto radius{ project.CardCornerRadius() / 1_mm };
    const auto margins{ project.ComputeMargins() / 1_mm };
    
    const auto page_size{ project.ComputePageSize() / 1_mm };
    const dla::vec2 available_space{
        page_size.x - margins.m_Left - margins.m_Right,
        page_size.y - margins.m_Top - margins.m_Bottom,
    };
    const dla::vec2 cards_offset{
        margins.m_Left + (available_space.x - cards_size.x) / 2.0f,
        margins.m_Top + (available_space.y - cards_size.y) / 2.0f,
    };

    for (const auto& transform : transforms)
    {
        const dla::vec2 position{ transform.m_Position.x / 1_mm, cards_size.y - transform.m_Position.y / 1_mm };
        const auto card_size{ transform.m_Size / 1_mm - bleed_edge * 2.0f };

        const auto left{ position.x + bleed_edge - cards_offset.x };
        const auto right{ left + card_size.x };
        const auto top{ position.y + bleed_edge - cards_offset.y };
        const auto bottom{ top - card_size.y };

        auto poly_line{ start_poly_line() };

        const auto draw_quarter_circle{
            [&](const dla::vec2 center,
                const float radius,
                const float start_angle)
            {
                static constexpr auto c_Resolution{ 32 };
                for (size_t i = 1; i < c_Resolution; i++)
                {
                    static constexpr float c_PiOver2{ std::numbers::pi_v<float> / 2 };
                    static constexpr float c_DegToRad{ std::numbers::pi_v<float> / 180 };
                    const float alpha{ start_angle * c_DegToRad + i * c_PiOver2 / c_Resolution };
                    const float sin_i{ std::sin(alpha) };
                    const float cos_i{ std::cos(alpha) };
                    draw_vertex({
                        center.x + sin_i * radius,
                        center.y + cos_i * radius,
                    });
                }
            },
        };

        draw_vertex({ right, top - radius });
        draw_vertex({ right, bottom + radius });
        draw_quarter_circle({ right - radius, bottom + radius }, radius, 90);
        draw_vertex({ right - radius, bottom });
        draw_vertex({ left + radius, bottom });
        draw_quarter_circle({ left + radius, bottom + radius }, radius, 180);
        draw_vertex({ left, bottom + radius });
        draw_vertex({ left, top - radius });
        draw_quarter_circle({ left + radius, top - radius }, radius, 270);
        draw_vertex({ left + radius, top });
        draw_vertex({ right - radius, top });
        draw_quarter_circle({ right - radius, top - radius }, radius, 0);
    }

    output << R"(  0
ENDSEC
  0
EOF
)";
}
