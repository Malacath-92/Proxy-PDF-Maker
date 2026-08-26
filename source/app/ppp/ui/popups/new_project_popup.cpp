#include <ppp/ui/popups/new_project_popup.hpp>

#include <ranges>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <ppp/config_types.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/widget_util/widget_combo_box.hpp>
#include <ppp/ui/widget_util/widget_label.hpp>

#include <ppp/ui/view_models/options/view_model_new_project_popup.hpp>

#include <ppp/profile/profile.hpp>

NewProjectPopup::NewProjectPopup(QWidget* parent,
                                 NewProjectPopupViewModel* view_model)
    : PopupBase{ parent }
    , m_ViewModel{ *view_model }
{
    m_ViewModel.setParent(this);

    m_AutoCenter = false;
    m_PersistGeometry = true;

    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("New Project Wizard");
    setObjectName("NewProjectWizard");

    auto* options{ new QWidget };
    {
        auto* project_name{ new LineEditWithLabel{ "Project Name", "new_project" } };
        m_ProjectName = project_name->GetWidget();

        m_ImageFolder = new QPushButton{ "images" };
        auto* image_folder{ new WidgetWithLabel{ "Image Folder", m_ImageFolder } };

        m_CardSize =
            MakeComboBox(
                m_ViewModel.GetCardSizes() | c_CardSizeNames,
                m_ViewModel.GetCardSizes() | c_CardSizeHints,
                m_ViewModel.GetDefaultCardSize());
        auto* card_size{
            new WidgetWithLabel{
                "Card Size",
                m_CardSize }
        };

        m_PaperSize =
            MakeComboBox(
                m_ViewModel.GetPageSizes() | c_PageSizeNames,
                m_ViewModel.GetDefaultPageSize());
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
                    m_ViewModel.ChangeImageFolder(ToQString(new_image_dir.value()));
                }
            }
        };

        connect(m_ProjectName, &QLineEdit::textChanged, m_ProjectName, set_project_name_tooltip);
        connect(m_ProjectName, &QLineEdit::textChanged, &m_ViewModel, &NewProjectPopupViewModel::ChangeProjectName);
        connect(m_ImageFolder, &QPushButton::clicked, this, browse_image_folder);
        connect(m_CardSize, &QComboBox::currentTextChanged, &m_ViewModel, &NewProjectPopupViewModel::ChangeCardSize);
        connect(m_PaperSize, &QComboBox::currentTextChanged, &m_ViewModel, &NewProjectPopupViewModel::ChangePaperSize);
        connect(m_ClearImages, &QCheckBox::checkStateChanged, &m_ViewModel, &NewProjectPopupViewModel::ChangeClearImages);
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
            &QDialog::close);
        QObject::connect(
            cancel_button,
            &QPushButton::clicked,
            &m_ViewModel,
            &NewProjectPopupViewModel::Cancel);
    }

    auto* window_layout{ new QVBoxLayout };
    window_layout->addWidget(options);
    window_layout->addWidget(buttons);

    setLayout(window_layout);

    m_ViewModel.ChangeProjectName(m_ProjectName->text());
    m_ViewModel.ChangeImageFolder(m_ImageFolder->text());
    m_ViewModel.ChangeCardSize(m_CardSize->currentText());
    m_ViewModel.ChangePaperSize(m_PaperSize->currentText());
    m_ViewModel.ChangeClearImages(m_ClearImages->checkState());
}
