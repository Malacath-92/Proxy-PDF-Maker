#include <ppp/ui/preview_overlays/widget_guides_overlay.hpp>

#include <QResizeEvent>

#include <ppp/project/project.hpp>
#include <ppp/svg/generate.hpp>

GuidesOverlay::GuidesOverlay(const Project& project, const PageImageTransforms& transforms)
    : m_Project{ project }
    , m_Transforms{ transforms }
{
    const auto& [darker_color, lighter_color]{
        CategorizeColors(project.m_Data.m_GuidesColorA,
                         project.m_Data.m_GuidesColorB)
    };

    m_PenOne.setColor(QColor{ darker_color.r, darker_color.g, darker_color.b });
    m_PenTwo.setColor(QColor{ lighter_color.r, lighter_color.g, lighter_color.b });
    m_PenTwo.setDashPattern({ 2.0f, 4.0f });

    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void GuidesOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{ this };
    painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);

    painter.setPen(m_PenOne);
    painter.drawLines(m_SolidLines);
    painter.drawLines(m_DashedLines);
    painter.setPen(m_PenTwo);
    painter.drawLines(m_DashedLines);

    painter.end();
}

void GuidesOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const dla::ivec2 size{ event->size().width(), event->size().height() };
    const auto pixel_ratio{ size / m_Project.ComputePageSize() };

    const auto guides_width{
        static_cast<int>(m_Project.m_Data.m_GuidesThickness * pixel_ratio.x)
    };
    m_PenOne.setWidth(guides_width);
    m_PenTwo.setWidth(guides_width);

    const auto line_length{ m_Project.m_Data.m_GuidesLength * pixel_ratio };
    const auto offset{ m_Project.m_Data.m_GuidesOffset * pixel_ratio };

    m_SolidLines.clear();
    m_DashedLines.clear();

    if (m_Transforms.empty())
    {
        return;
    }

    if (m_Project.m_Data.m_CornerGuides)
    {
        for (const auto& transform : m_Transforms)
        {
            const auto top_left_corner{ transform.m_Card.m_Position * pixel_ratio };
            const auto card_size{ transform.m_Card.m_Size * pixel_ratio };

            const auto draw_guide{
                [this](QLineF line, bool cross = false)
                {
                    if (cross)
                    {
                        const auto from{ line.p1() };
                        const auto delta{ line.p2() - from };
                        line.setP1(from - delta);
                    }
                    m_DashedLines.push_back(line);
                }
            };

            const auto top_left_pos{ top_left_corner + offset };
            const auto top_right_pos{ top_left_corner + dla::vec2(1.0f, 0.0f) * card_size + dla::vec2(-1.0f, 1.0f) * offset };
            const auto bottom_right_pos{ top_left_corner + dla::vec2(1.0f, 1.0f) * card_size + dla::vec2(-1.0f, -1.0f) * offset };
            const auto bottom_left_pos{ top_left_corner + dla::vec2(0.0f, 1.0f) * card_size + dla::vec2(1.0f, -1.0f) * offset };

            draw_guide(QLineF{ top_left_pos.x,
                               top_left_pos.y,
                               top_left_pos.x + line_length.x,
                               top_left_pos.y },
                       m_Project.m_Data.m_CrossGuides);
            draw_guide(QLineF{ top_left_pos.x,
                               top_left_pos.y,
                               top_left_pos.x,
                               top_left_pos.y + line_length.y },
                       m_Project.m_Data.m_CrossGuides);

            draw_guide(QLineF{ top_right_pos.x,
                               top_right_pos.y,
                               top_right_pos.x - line_length.x,
                               top_right_pos.y },
                       m_Project.m_Data.m_CrossGuides);
            draw_guide(QLineF{ top_right_pos.x,
                               top_right_pos.y,
                               top_right_pos.x,
                               top_right_pos.y + line_length.y },
                       m_Project.m_Data.m_CrossGuides);

            draw_guide(QLineF{ bottom_right_pos.x,
                               bottom_right_pos.y,
                               bottom_right_pos.x - line_length.x,
                               bottom_right_pos.y },
                       m_Project.m_Data.m_CrossGuides);
            draw_guide(QLineF{ bottom_right_pos.x,
                               bottom_right_pos.y,
                               bottom_right_pos.x,
                               bottom_right_pos.y - line_length.y },
                       m_Project.m_Data.m_CrossGuides);

            draw_guide(QLineF{ bottom_left_pos.x,
                               bottom_left_pos.y,
                               bottom_left_pos.x + line_length.x,
                               bottom_left_pos.y },
                       m_Project.m_Data.m_CrossGuides);
            draw_guide(QLineF{ bottom_left_pos.x,
                               bottom_left_pos.y,
                               bottom_left_pos.x,
                               bottom_left_pos.y - line_length.y },
                       m_Project.m_Data.m_CrossGuides);
        }
    }

    if (m_Project.m_Data.m_ExtendedGuides)
    {
        static constexpr auto c_Precision{ 0.1_pts };

        std::vector<int32_t> unique_x;
        std::vector<int32_t> unique_y;
        for (const auto& transform : m_Transforms)
        {
            const auto top_left_corner{
                static_cast<dla::ivec2>((transform.m_Card.m_Position + offset / pixel_ratio) / c_Precision)
            };
            if (!std::ranges::contains(unique_x, top_left_corner.x))
            {
                unique_x.push_back(top_left_corner.x);
            }
            if (!std::ranges::contains(unique_y, top_left_corner.y))
            {
                unique_y.push_back(top_left_corner.y);
            }

            const auto card_size{
                static_cast<dla::ivec2>((transform.m_Card.m_Size - offset * 2 / pixel_ratio) / c_Precision)
            };
            const auto bottom_right_corner{ top_left_corner + card_size };
            if (!std::ranges::contains(unique_x, bottom_right_corner.x))
            {
                unique_x.push_back(bottom_right_corner.x);
            }
            if (!std::ranges::contains(unique_y, bottom_right_corner.y))
            {
                unique_y.push_back(bottom_right_corner.y);
            }
        }

        const auto bleed{ m_Project.m_Data.m_BleedEdge };
        const auto envelope_bleed{ m_Project.m_Data.m_EnvelopeBleedEdge };
        const auto extended_off{ offset + (bleed + envelope_bleed + 1_mm) * pixel_ratio };
        const auto x_min{ std::ranges::min(unique_x) * c_Precision * pixel_ratio.x - extended_off.x };
        const auto x_max{ std::ranges::max(unique_x) * c_Precision * pixel_ratio.x + extended_off.y };
        const auto y_min{ std::ranges::min(unique_y) * c_Precision * pixel_ratio.y - extended_off.x };
        const auto y_max{ std::ranges::max(unique_y) * c_Precision * pixel_ratio.y + extended_off.y };

        for (const auto& x : unique_x)
        {
            const auto real_x{ x * c_Precision * pixel_ratio.x };
            m_SolidLines.push_back(QLineF{ real_x, y_min, real_x, 0.0f });
            m_SolidLines.push_back(QLineF{ real_x, y_max, real_x, static_cast<float>(size.y) });
        }

        for (const auto& y : unique_y)
        {
            const auto real_y{ y * c_Precision * pixel_ratio.y };
            m_SolidLines.push_back(QLineF{ x_min, real_y, 0.0f, real_y });
            m_SolidLines.push_back(QLineF{ x_max, real_y, static_cast<float>(size.x), real_y });
        }
    }
}
