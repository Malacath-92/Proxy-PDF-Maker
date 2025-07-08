#include <ppp/svg/generate.hpp>

#include <numbers>

#include <QPen>
#include <QRect>
#include <QSvgGenerator>

#include <ppp/qt_util.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

void DrawSvg(QPainter& painter, const QPainterPath& path)
{
    painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);

    QPen pen{};
    pen.setWidth(1);
    pen.setColor(QColor{ 255, 0, 0 });
    painter.setPen(pen);
    painter.drawPath(path);
}

QPainterPath GenerateCardsPath(const Project& project)
{
    const auto svg_size{ project.ComputeCardsSize() / 1_mm };
    return GenerateCardsPath(dla::vec2{ 0.0f, 0.0f },
                             svg_size,
                             project);
}

QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 size,
                               const Project& project)
{
    return GenerateCardsPath(origin,
                             size,
                             project.m_Data.m_CardLayout,
                             project.CardSize(),
                             project.m_Data.m_BleedEdge,
                             project.m_Data.m_Spacing,
                             project.CardCornerRadius());
}

QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 size,
                               dla::uvec2 grid,
                               Size card_size,
                               Length bleed_edge,
                               Length spacing,
                               Length corner_radius)
{
    const auto card_size_with_bleed{ card_size + 2 * bleed_edge };
    const auto physical_canvas_size{
        grid * card_size_with_bleed + (grid - 1) * spacing
    };
    const auto pixel_ratio{ size / physical_canvas_size };
    const auto card_size_with_bleed_pixels{ card_size_with_bleed * pixel_ratio };
    const auto corner_radius_pixels{ corner_radius * pixel_ratio };
    const auto bleed_pixels{ bleed_edge * pixel_ratio };
    const auto spacing_pixels{ spacing * pixel_ratio };
    const auto card_size_pixels{ card_size_with_bleed_pixels - 2 * bleed_pixels };

    QPainterPath card_border;
    if (bleed_edge > 0_mm)
    {
        const QRectF rect{
            origin.x,
            origin.y,
            size.x,
            size.y,
        };
        card_border.addRect(rect);
    }

    const auto& [columns, rows]{ grid.pod() };
    for (uint32_t x = 0; x < columns; x++)
    {
        for (uint32_t y = 0; y < rows; y++)
        {
            const dla::uvec2 idx{ x, y };
            const auto top_left_corner{
                origin + idx * (card_size_with_bleed_pixels + spacing_pixels) + bleed_pixels
            };
            const QRectF rect{
                top_left_corner.x,
                top_left_corner.y,
                card_size_pixels.x,
                card_size_pixels.y,
            };
            card_border.addRoundedRect(rect, corner_radius_pixels.x, corner_radius_pixels.y);
        }
    }

    return card_border;
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

    if (project.m_Data.m_BleedEdge > 0_mm)
    {
        const auto cards_size{ project.ComputeCardsSize() / 1_mm };
        auto poly_line{ start_poly_line() };

        draw_vertex(dla::vec2{ 0, 0 });
        draw_vertex(dla::vec2{ cards_size.x, 0 });
        draw_vertex(dla::vec2{ cards_size.x, cards_size.y });
        draw_vertex(dla::vec2{ 0, cards_size.y });
    }

    const auto radius{ project.CardCornerRadius() / 1_mm };
    const auto bleed{ project.m_Data.m_BleedEdge / 1_mm };
    const auto spacing{ project.m_Data.m_Spacing / 1_mm };
    const auto card_size_with_bleed{ project.CardSizeWithBleed() / 1_mm };
    const auto card_size_without_bleed{ project.CardSize() / 1_mm };
    const auto grid{ project.m_Data.m_CardLayout };
    const auto& [columns, rows]{ grid.pod() };

    const dla::vec2 origin{ spacing, spacing };

    for (uint32_t x = 0; x < columns; x++)
    {
        for (uint32_t y = 0; y < rows; y++)
        {
            const dla::uvec2 idx{ x, y };
            const auto top_left_corner{
                origin + idx * (card_size_with_bleed + spacing) + bleed
            };
            const auto bottom_right_corner{
                top_left_corner + card_size_without_bleed
            };

            auto poly_line{ start_poly_line() };

            const auto draw_quarter_circle{
                [&](const dla::vec2 center,
                    const float radius,
                    const float start_angle)
                {
                    static constexpr auto c_Resolution{ 32 };
                    for (size_t i = 1; i < c_Resolution; i++)
                    {
                        static constexpr float pi_over_2{ std::numbers::pi_v<float> / 2 };
                        static constexpr float deg_to_rad{ std::numbers::pi_v<float> / 180 };
                        const float alpha{ start_angle * deg_to_rad + i * pi_over_2 / c_Resolution };
                        const float sin_i{ sin(alpha) };
                        const float cos_i{ cos(alpha) };
                        draw_vertex({
                            center.x + sin_i * radius,
                            center.y + cos_i * radius,
                        });
                    }
                },
            };

            draw_vertex({ top_left_corner.x, top_left_corner.y + radius });
            draw_vertex({ top_left_corner.x, bottom_right_corner.y - radius });
            draw_quarter_circle({ top_left_corner.x + radius, bottom_right_corner.y - radius }, radius, 270);
            draw_vertex({ top_left_corner.x + radius, bottom_right_corner.y });
            draw_vertex({ bottom_right_corner.x - radius, bottom_right_corner.y });
            draw_quarter_circle({ bottom_right_corner.x - radius, bottom_right_corner.y - radius }, radius, 0);
            draw_vertex({ bottom_right_corner.x, bottom_right_corner.y - radius });
            draw_vertex({ bottom_right_corner.x, top_left_corner.y + radius });
            draw_quarter_circle({ bottom_right_corner.x - radius, top_left_corner.y + radius }, radius, 90);
            draw_vertex({ bottom_right_corner.x - radius, top_left_corner.y });
            draw_vertex({ top_left_corner.x + radius, top_left_corner.y });
            draw_quarter_circle({ top_left_corner.x + radius, top_left_corner.y + radius }, radius, 180);
        }
    }

    output << R"(  0
ENDSEC
  0
EOF
)";
}
