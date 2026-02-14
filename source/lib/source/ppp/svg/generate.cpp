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
    pen.setWidth(0.1);
    pen.setColor(color);
    painter.setPen(pen);
    painter.drawPath(path);
}

QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 pixel_size,
                               const Project& project)
{
    QPainterPath card_border;
    if (project.m_Data.m_BleedEdge > 0_mm || project.m_Data.m_EnvelopeBleedEdge > 0_mm)
    {
        const QRectF rect{
            0.0f,
            0.0f,
            pixel_size.x,
            pixel_size.y,
        };
        card_border.addRect(rect);
    }

    const auto cards_size{ project.ComputeCardsSize() };
    const auto pixel_ratio{ pixel_size / cards_size };

    const auto transforms{ ComputeTransforms(project) };
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

    for (const auto& transform : transforms)
    {
        if (project.IsCardSvg())
        {
            DrawSvgToPainterPath(card_border,
                                 project.CardSvgData(),
                                 transform.m_Card.m_Position - cards_origin,
                                 transform.m_Card.m_Size,
                                 1.0f / pixel_ratio);
        }
        else
        {
            const auto top_left_corner{ (transform.m_Card.m_Position - cards_origin) * pixel_ratio };
            const auto card_size{ transform.m_Card.m_Size * pixel_ratio };

            const QRectF rect{
                top_left_corner.x,
                top_left_corner.y,
                card_size.x,
                card_size.y,
            };
            if (project.IsCardRoundedRect())
            {
                card_border.addRoundedRect(rect, corner_radius.x, corner_radius.y);
            }
            else
            {
                card_border.addRect(rect);
            }
        }
    }

    card_border.translate(origin.x, origin.y);

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

    const auto svg_dpi{ 600 };
    const auto mm_to_in{ 1_in / 1_mm };
    const auto unit_conv{ svg_dpi / mm_to_in };
    const auto cards_size{ project.ComputeCardsSize() / 1_mm * unit_conv };
    const auto page_size{ project.ComputePageSize() / 1_mm * unit_conv };
    const auto margins{ project.ComputeMargins() / 1_mm * unit_conv };

    LogInfo("Generating card path...");
    const QPainterPath path{ GenerateCardsPath(dla::vec2{ margins.m_Left, margins.m_Top }, cards_size, project) };
    const QSize svg_size{ static_cast<int>(page_size.x), static_cast<int>(page_size.y) };
    const QSizeF viewbox_size{ page_size.x, page_size.y };

    QSvgGenerator generator{};
    generator.setFileName(ToQString(svg_path));
    generator.setTitle("Card cutting guides.");
    generator.setDescription("An SVG containing exact cutting guides for the accompanying sheet.");
    generator.setSize(QSize{ static_cast<int>(page_size.x), static_cast<int>(page_size.y) });
    generator.setViewBox(QRectF{ QPointF{ 0, 0 }, viewbox_size });
    generator.setResolution(svg_dpi);

    LogInfo("Drawing card path...");
    QPainter painter;
    painter.begin(&generator);
    DrawSvg(painter, path);
    painter.end();
}

void GenerateCardsDxf(const Project& project)
{
    /*
     * Tbh, .dxf files kinda suck, but some tools will require subscriptions for importiong .svg files, so here we are
     * A .dxf file works like this: (shitty tl;dr incoming)
     *   a single line group code
     *   followed by a value for that code
     * Some entities will expect certain values following it, always preceeded by the corresponding group code
     * Group codes used here are:
     *   0       : Entity Type
     *   8       : Layer Name
     *   9       : Variable Name ID (used in HEADER)
     *   10      : Primary Point, aka X value
     *   20      : Y value
     *   66      : "Entities Follow" flag
     *   70 - 78 : Various integer values
     */
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
1
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
HEADER
  9
$MEASUREMENT
  70
1
  9
$INSUNITS
  70
4
  0
ENDSEC
  0
SECTION
  2
ENTITIES
)";

    const auto cards_size{ project.ComputeCardsSize() / 1_mm };
    if (project.m_Data.m_BleedEdge > 0_mm || project.m_Data.m_EnvelopeBleedEdge > 0_mm)
    {
        auto poly_line{ start_poly_line() };

        draw_vertex(dla::vec2{ 0, 0 });
        draw_vertex(dla::vec2{ cards_size.x, 0 });
        draw_vertex(dla::vec2{ cards_size.x, cards_size.y });
        draw_vertex(dla::vec2{ 0, cards_size.y });
    }

    const auto transforms{ ComputeTransforms(project) };
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
        const auto top_left_corner{ transform.m_Card.m_Position / 1_mm };
        const auto card_size{ transform.m_Card.m_Size / 1_mm };

        const auto left{ top_left_corner.x - cards_offset.x };
        const auto right{ left + card_size.x };
        const auto top{ cards_size.y - (top_left_corner.y - cards_offset.y) };
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
