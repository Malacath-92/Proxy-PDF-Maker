#include <ppp/ui/popups/new_project_popup.hpp>

#include <ranges>

#include <QApplication>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#include <ppp/qt_util.hpp>

#include <ppp/app.hpp>
#include <ppp/project/project.hpp>
#include <ppp/ui/widget_combo_box.hpp>
#include <ppp/ui/widget_label.hpp>

#include <ppp/profile/profile.hpp>

NewProjectPopup::NewProjectPopup(QWidget* parent)
    : PopupBase{ parent }
{
    m_AutoCenter = false;
    m_PersistGeometry = true;

    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("New Project Wizard");
    setObjectName("NewProjectWizard");

    auto* options{ new QWidget };
    {
        auto* project_name{ new LineEditWithLabel{ "Project Name", "new_project" } };
        m_ProjectName = project_name->GetWidget();

        m_ImageFolder = new QPushButton{ m_ActualImageFolder };
        auto* image_folder{ new WidgetWithLabel{ "Image Folder", m_ImageFolder } };

        const auto default_card_size{
            []() -> std::string
            {
                auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
                auto user_default{ app->GetProjectDefault("card_size") };
                if (!user_default.is_null())
                {
                    return user_default;
                }
                else
                {
                    return ProjectData{}.m_CardSizeChoice;
                }
            }()
        };
        m_CardSize =
            MakeComboBox(
                std::span<const std::string>{ std::views::keys(g_Cfg.m_CardSizes) |
                                              std::ranges::to<std::vector>() },
                std::span<const std::string>{ g_Cfg.m_CardSizes |
                                              std::views::values |
                                              std::views::transform(&Config::CardSizeInfo::m_Hint) |
                                              std::ranges::to<std::vector>() },
                default_card_size);
        auto* card_size{
            new WidgetWithLabel{
                "Card Size",
                m_CardSize }
        };

        const auto default_page_size{
            []() -> std::string
            {
                auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
                auto user_default{ app->GetProjectDefault("page_size") };
                if (!user_default.is_null())
                {
                    return user_default;
                }
                else
                {
                    return ProjectData{}.m_PageSize;
                }
            }()
        };
        m_PaperSize =
            MakeComboBox(
                std::span<const std::string>{ std::views::keys(g_Cfg.m_PageSizes) |
                                              std::ranges::to<std::vector>() },
                {},
                default_page_size);
        auto* paper_size{
            new WidgetWithLabel{
                "Paper Size",
                m_PaperSize }
        };

        m_ClearImages = new QCheckBox{ "Clear Image Folder" };

        auto* options_layout{ new QVBoxLayout };
        options_layout->addWidget(project_name);
        options_layout->addWidget(image_folder);
        options_layout->addWidget(card_size);
        options_layout->addWidget(paper_size);
        options_layout->addWidget(m_ClearImages);

        options->setLayout(options_layout);

        const auto set_project_name_tooltip{
            [this]()
            {
                m_ProjectName->setToolTip(
                    QString{ "The project file with be this %1.json" }
                        .arg(m_ProjectName->text()));
            }
        };
        set_project_name_tooltip();

        const auto browse_image_folder{
            [this]()
            {
                TRACY_AUTO_SCOPE();
                if (const auto new_image_dir{ OpenFolderDialog(".") })
                {
                    m_ImageFolder->setText(ToQString(new_image_dir.value().filename()));
                    m_ActualImageFolder = ToQString(new_image_dir.value());
                }
            }
        };

        connect(m_ProjectName, &QLineEdit::textChanged, m_ProjectName, set_project_name_tooltip);
        connect(m_ImageFolder, &QPushButton::clicked, this, browse_image_folder);
    }

    auto* buttons{ new QWidget };
    {
        auto* create_button{ new QPushButton{ "Create" } };
        auto* cancel_button{ new QPushButton{ "Cancel" } };

        auto* buttons_layout{ new QHBoxLayout };
        buttons_layout->addWidget(create_button);
        buttons_layout->addWidget(cancel_button);

        buttons->setLayout(buttons_layout);

        QTimer::singleShot(0, this, [create_button]
                           { create_button->setFocus(); });

        QObject::connect(
            create_button,
            &QPushButton::clicked,
            this,
            &QDialog::close);
        QObject::connect(
            cancel_button,
            &QPushButton::clicked,
            this,
            [this]()
            {
                m_Cancelled = true;
                close();
            });
    }

    auto* window_layout{ new QVBoxLayout };
    window_layout->addWidget(options);
    window_layout->addWidget(buttons);

    setLayout(window_layout);
}

bool NewProjectPopup::CreateNewProject() const
{
    return !m_Cancelled;
}

QString NewProjectPopup::NewProjectName() const
{
    return m_ProjectName->text();
}

QString NewProjectPopup::NewImageFolder() const
{
    return m_ActualImageFolder;
}

QString NewProjectPopup::NewCardSize() const
{
    return m_CardSize->currentText();
}

QString NewProjectPopup::NewPaperSize() const
{
    return m_PaperSize->currentText();
}

bool NewProjectPopup::ClearImages() const
{
    return m_ClearImages->isChecked();
}
