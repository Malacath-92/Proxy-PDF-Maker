#include <ppp/ui/widget_guides_options.hpp>

#include <QCheckBox>
#include <QColorDialog>
#include <QPushButton>
#include <QVBoxLayout>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/widget_label.hpp>

GuidesOptionsWidget::GuidesOptionsWidget(Project& project)
    : QDockWidget{ "GuidesOptions" }
    , m_Project{ project }
{
    setFeatures(DockWidgetMovable | DockWidgetFloatable);
    setAllowedAreas(Qt::AllDockWidgetAreas);

    const auto color_to_bg_style{
        [](const ColorRGB8& color)
        {
            return ToQString(fmt::format(":enabled {{ background-color:#{:0>6x}; }}", ColorToInt(color)));
        }
    };

    auto* export_exact_guides_checkbox{ new QCheckBox{ "Export Exact Guides" } };
    export_exact_guides_checkbox->setChecked(project.Data.ExportExactGuides);
    export_exact_guides_checkbox->setToolTip("Decides whether a .svg file will be generated that contains the exact guides for the current layout");

    auto* enable_guides_checkbox{ new QCheckBox{ "Enable Guides" } };
    enable_guides_checkbox->setChecked(project.Data.EnableGuides);
    auto* extended_guides_checkbox{ new QCheckBox{ "Extended Guides" } };
    extended_guides_checkbox->setChecked(project.Data.ExtendedGuides);
    extended_guides_checkbox->setEnabled(project.Data.EnableGuides);
    auto* guides_color_a_button{ new QPushButton };
    guides_color_a_button->setStyleSheet(color_to_bg_style(project.Data.GuidesColorA));
    auto* guides_color_a{ new WidgetWithLabel{ "Guides Color A", guides_color_a_button } };
    guides_color_a->setEnabled(project.Data.EnableGuides);
    auto* guides_color_b_button{ new QPushButton };
    guides_color_b_button->setStyleSheet(color_to_bg_style(project.Data.GuidesColorB));
    auto* guides_color_b{ new WidgetWithLabel{ "Guides Color B", guides_color_b_button } };
    guides_color_b->setEnabled(project.Data.EnableGuides);

    auto* layout{ new QVBoxLayout };
    layout->addWidget(export_exact_guides_checkbox);
    layout->addWidget(enable_guides_checkbox);
    layout->addWidget(extended_guides_checkbox);
    layout->addWidget(guides_color_a);
    layout->addWidget(guides_color_b);

    auto* main_widget{ new QWidget };
    main_widget->setLayout(layout);
    main_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setWidget(main_widget);

    auto main_window{
        [this]()
        { return static_cast<PrintProxyPrepMainWindow*>(window()); }
    };

    auto change_export_exact_guides{
        [=, this](Qt::CheckState s)
        {
            const bool enabled{ s == Qt::CheckState::Checked };
            m_Project.Data.ExportExactGuides = enabled;

            main_window()->ExactGuidesEnabledChanged(m_Project);
        }
    };

    auto change_enable_guides{
        [=, this](Qt::CheckState s)
        {
            const bool enabled{ s == Qt::CheckState::Checked };
            m_Project.Data.EnableGuides = enabled;

            extended_guides_checkbox->setEnabled(enabled);
            guides_color_a->setEnabled(enabled);
            guides_color_b->setEnabled(enabled);

            main_window()->GuidesEnabledChanged(m_Project);
        }
    };

    auto change_extended_guides{
        [=, this](Qt::CheckState s)
        {
            m_Project.Data.ExtendedGuides = s == Qt::CheckState::Checked;
        }
    };

    auto pick_color{
        [](const ColorRGB8& color) -> std::optional<ColorRGB8>
        {
            const QColor initial_color{ color.r, color.g, color.b };
            const QColor picked_color{ QColorDialog::getColor(initial_color) };
            if (picked_color.isValid())
            {
                const std::string new_color{ picked_color.name().toStdString() };
                uint32_t color_uint{};
                std::from_chars(new_color.c_str() + 1, new_color.c_str() + new_color.size(), color_uint, 16);
                return ColorRGB8{
                    static_cast<uint8_t>((color_uint >> 16) & 0xff),
                    static_cast<uint8_t>((color_uint >> 8) & 0xff),
                    static_cast<uint8_t>(color_uint & 0xff),
                };
            }
            return std::nullopt;
        }
    };

    auto pick_color_a{
        [=, this]()
        {
            if (const auto picked_color{ pick_color(m_Project.Data.GuidesColorA) })
            {
                m_Project.Data.GuidesColorA = picked_color.value();
                guides_color_a_button->setStyleSheet(color_to_bg_style(m_Project.Data.GuidesColorA));
                main_window()->GuidesColorChanged(m_Project);
            }
        }
    };

    auto pick_color_b{
        [=, this]()
        {
            if (const auto picked_color{ pick_color(m_Project.Data.GuidesColorB) })
            {
                m_Project.Data.GuidesColorB = picked_color.value();
                guides_color_b_button->setStyleSheet(color_to_bg_style(m_Project.Data.GuidesColorB));
                main_window()->GuidesColorChanged(m_Project);
            }
        }
    };

    QObject::connect(export_exact_guides_checkbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_export_exact_guides);
    QObject::connect(enable_guides_checkbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_enable_guides);
    QObject::connect(extended_guides_checkbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_extended_guides);
    QObject::connect(guides_color_a_button,
                     &QPushButton::clicked,
                     this,
                     pick_color_a);
    QObject::connect(guides_color_b_button,
                     &QPushButton::clicked,
                     this,
                     pick_color_b);
}
