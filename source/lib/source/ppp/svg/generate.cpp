
#include <ppp/svg/generate.hpp>

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
    QPainterPath path{ GenerateCardsPath(project) };

    QSvgGenerator generator{};
    generator.setFileName(ToQString(svg_path));
    generator.setTitle("Card cutting guides.");
    generator.setDescription("An SVG containing exact cutting guides for the accompaniying sheet.");

    LogInfo("Drawing card path...");
    QPainter painter;
    painter.begin(&generator);
    DrawSvg(painter, path);
    painter.end();
}