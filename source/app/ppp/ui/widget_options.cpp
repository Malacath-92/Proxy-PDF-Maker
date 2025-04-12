#include <ppp/ui/widget_options.hpp>

#include <atomic>
#include <cstdint>
#include <ranges>

#include <QCheckBox>
#include <QColorDialog>
#include <QCursor>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <magic_enum/magic_enum.hpp>

#include <dla/vector_math.h>

#include <ppp/app.hpp>
#include <ppp/cubes.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/style.hpp>

#include <ppp/pdf/generate.hpp>
#include <ppp/pdf/util.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_card.hpp>
#include <ppp/ui/widget_label.hpp>

class ActionsWidget : public QGroupBox
{
  public:
    ActionsWidget(PrintProxyPrepApplication& application, Project& project)
    {
        setTitle("Actions");

        auto* cropper_progress_bar{ new QProgressBar };
        cropper_progress_bar->setToolTip("Cropper Progress");
        cropper_progress_bar->setTextVisible(false);
        cropper_progress_bar->setVisible(false);
        cropper_progress_bar->setRange(0, ProgressBarResolution);
        auto* render_button{ new QPushButton{ "Render Document" } };
        auto* save_button{ new QPushButton{ "Save Project" } };
        auto* load_button{ new QPushButton{ "Load Project" } };
        auto* set_images_button{ new QPushButton{ "Set Image Folder" } };
        auto* open_images_button{ new QPushButton{ "Open Images" } };

        const QWidget* buttons[]{
            cropper_progress_bar,
            render_button,
            save_button,
            load_button,
            set_images_button,
            open_images_button,
        };

        auto widths{ buttons | std::views::transform([](const QWidget* widget)
                                                     { return widget->sizeHint().width(); }) };
        const int32_t minimum_width{ *std::ranges::max_element(widths) };

        auto* layout{ new QGridLayout };
        layout->setColumnMinimumWidth(0, minimum_width + 10);
        layout->setColumnMinimumWidth(1, minimum_width + 10);
        layout->addWidget(cropper_progress_bar, 0, 0, 1, 2);
        layout->addWidget(render_button, 1, 0, 1, 2);
        layout->addWidget(save_button, 2, 0);
        layout->addWidget(load_button, 2, 1);
        layout->addWidget(set_images_button, 3, 0);
        layout->addWidget(open_images_button, 3, 1);
        setLayout(layout);

        const auto render{
            [=, this, &application, &project]()
            {
                GenericPopup render_window{ window(), "Rendering PDF..." };

                const auto render_work{
                    [=, &project, &application, &render_window]()
                    {
                        const auto print_fn{ render_window.MakePrintFn() };

                        {
                            std::vector<fs::path> used_cards{};
                            for (const auto& [img, info] : project.Data.Cards)
                            {
                                if (info.Num > 0)
                                {
                                    used_cards.push_back(img);
                                    if (project.Data.BacksideEnabled && !std::ranges::contains(used_cards, info.Backside))
                                    {
                                        used_cards.push_back(info.Backside.empty() ? project.Data.BacksideDefault : info.Backside);
                                    }
                                }
                            }

                            const Length bleed_edge{ project.Data.BleedEdge };
                            const fs::path& image_dir{ project.Data.ImageDir };
                            const fs::path& crop_dir{ project.Data.CropDir };
                            if (NeedRunMinimalCropper(image_dir, crop_dir, used_cards, bleed_edge, CFG.ColorCube))
                            {
                                RunMinimalCropper(image_dir, crop_dir, used_cards, bleed_edge, CFG.MaxDPI, CFG.ColorCube, GetCubeImage(application, CFG.ColorCube), print_fn);
                            }
                        }

                        if (auto file_path{ GeneratePdf(project, print_fn) })
                        {
                            OpenFile(file_path.value());
                        }
                        else
                        {
                            QToolTip::showText(QCursor::pos(), "Failure while creating pdf...");
                        }
                    }
                };

                window()->setEnabled(false);
                render_window.ShowDuringWork(render_work);
                window()->setEnabled(true);
            }
        };

        const auto save_project{
            [=, &project, &application]()
            {
                if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Save) })
                {
                    application.SetProjectPath(new_project_json.value());
                    project.Dump(new_project_json.value(), nullptr);
                }
            }
        };

        const auto load_project{
            [=, this, &project, &application]()
            {
                if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Open) })
                {
                    if (new_project_json != application.GetProjectPath())
                    {
                        application.SetProjectPath(new_project_json.value());
                        GenericPopup reload_window{ window(), "Reloading project..." };

                        const auto load_project_work{
                            [=, &project, &reload_window]()
                            {
                                project.Load(new_project_json.value(), reload_window.MakePrintFn());
                            }
                        };

                        auto main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };

                        main_window->setEnabled(false);
                        reload_window.ShowDuringWork(load_project_work);
                        main_window->NewProjectOpenedDiff(project.Data);
                        main_window->NewProjectOpened(project);
                        main_window->setEnabled(true);
                    }
                }
            }
        };

        const auto set_images_folder{
            [=, this, &project, &application]()
            {
                if (const auto new_image_dir{ OpenFolderDialog(".") })
                {
                    if (new_image_dir != project.Data.ImageDir)
                    {
                        project.Data.ImageDir = new_image_dir.value();
                        project.Data.CropDir = project.Data.ImageDir / "crop";
                        project.Data.ImageCache = project.Data.CropDir / "preview.cache";

                        project.Init(nullptr);

                        auto main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
                        main_window->ImageDirChangedDiff(project.Data.ImageDir,
                                                         project.Data.CropDir,
                                                         project.Data.Previews | std::views::keys | std::ranges::to<std::vector>());
                        main_window->ImageDirChanged(project);
                    }
                }
            }
        };

        const auto open_images_folder{
            [=, &project]()
            {
                OpenFolder(project.Data.ImageDir);
            }
        };

        QObject::connect(render_button,
                         &QPushButton::clicked,
                         this,
                         render);
        QObject::connect(save_button,
                         &QPushButton::clicked,
                         this,
                         save_project);
        QObject::connect(load_button,
                         &QPushButton::clicked,
                         this,
                         load_project);
        QObject::connect(set_images_button,
                         &QPushButton::clicked,
                         this,
                         set_images_folder);
        QObject::connect(open_images_button,
                         &QPushButton::clicked,
                         this,
                         open_images_folder);

        CropperProgressBar = cropper_progress_bar;
        RenderButton = render_button;
    }

    void CropperWorking()
    {
        CropperProgressBar->setVisible(true);
        CropperProgressBar->setValue(0);
        RenderButton->setVisible(false);
    }

    void CropperDone()
    {
        CropperProgressBar->setVisible(false);
        RenderButton->setVisible(true);
    }

    void CropperProgress(float progress)
    {
        const int progress_whole{ static_cast<int>(progress * ProgressBarResolution) };
        CropperProgressBar->setValue(progress_whole);
    }

  private:
    static inline constexpr int ProgressBarResolution{ 250 };
    QProgressBar* CropperProgressBar;
    QWidget* RenderButton;
};

class PrintOptionsWidget : public QGroupBox
{
  public:
    PrintOptionsWidget(Project& project)
    {
        setTitle("Print Options");

        const auto color_to_bg_style{
            [](const ColorRGB8& color)
            {
                return ToQString(fmt::format(":enabled {{ background-color:#{:0>6x}; }}", ColorToInt(color)));
            }
        };

        const bool fit_size{ project.Data.PageSize == Config::FitSize };
        const bool infer_size{ project.Data.PageSize == Config::BasePDFSize };

        const auto max_margins{
            [&]()
            {
                const Size card_size_with_bleed{ CardSizeWithoutBleed + 2 * project.Data.BleedEdge };
                const Size full_size{ card_size_with_bleed * project.Data.CardLayout };
                return project.Data.PageSizePhysical - full_size;
            }()
        };

        using namespace std::string_view_literals;
        auto* print_output{ new LineEditWithLabel{ "Output &Filename", project.Data.FileName.string() } };
        auto* paper_size{ new ComboBoxWithLabel{
            "&Paper Size", std::views::keys(CFG.PageSizes) | std::ranges::to<std::vector>(), project.Data.PageSize } };

        auto* base_pdf_choice{ new ComboBoxWithLabel{
            "&Base Pdf", GetBasePdfNames(), project.Data.BasePdf } };
        auto* base_pdf_info{ new LabelWithLabel{ "Size", PageSizeToString(project.Data.PageSizePhysical) } };
        auto* base_pdf_top_margin{ new DoubleSpinBoxWithLabel{ "&Top Margin" } };
        auto* base_pdf_top_margin_spin{ base_pdf_top_margin->GetWidget() };
        base_pdf_top_margin_spin->setDecimals(2);
        base_pdf_top_margin_spin->setSingleStep(0.1);
        base_pdf_top_margin_spin->setSuffix("mm");
        base_pdf_top_margin_spin->setRange(0, max_margins.x / 1_mm);
        base_pdf_top_margin_spin->setValue(project.Data.CustomMargins.x / 1_mm);
        auto* base_pdf_left_margin{ new DoubleSpinBoxWithLabel{ "&Left Margin" } };
        auto* base_pdf_left_margin_spin{ base_pdf_left_margin->GetWidget() };
        base_pdf_left_margin_spin->setDecimals(2);
        base_pdf_left_margin_spin->setSingleStep(0.1);
        base_pdf_left_margin_spin->setSuffix("mm");
        base_pdf_left_margin_spin->setRange(0, max_margins.y / 1_mm);
        base_pdf_left_margin_spin->setValue(project.Data.CustomMargins.y / 1_mm);
        auto* base_pdf_layout{ new QVBoxLayout };
        base_pdf_layout->addWidget(base_pdf_choice);
        base_pdf_layout->addWidget(base_pdf_info);
        base_pdf_layout->addWidget(base_pdf_top_margin);
        base_pdf_layout->addWidget(base_pdf_left_margin);
        base_pdf_layout->setContentsMargins(0, 0, 0, 0);
        auto* base_pdf{ new QWidget };
        base_pdf->setLayout(base_pdf_layout);
        base_pdf->setEnabled(infer_size);
        base_pdf->setVisible(infer_size);

        auto* cards_width{ new QDoubleSpinBox };
        cards_width->setDecimals(0);
        cards_width->setRange(1, 10);
        cards_width->setSingleStep(1);
        cards_width->setValue(project.Data.CardLayout.x);
        auto* cards_height{ new QDoubleSpinBox };
        cards_height->setDecimals(0);
        cards_height->setRange(1, 10);
        cards_height->setSingleStep(1);
        cards_height->setValue(project.Data.CardLayout.y);
        auto* cards_layout_layout{ new QHBoxLayout };
        cards_layout_layout->addWidget(cards_width);
        cards_layout_layout->addWidget(cards_height);
        cards_layout_layout->setContentsMargins(0, 0, 0, 0);
        auto* cards_layout_container{ new QWidget };
        cards_layout_container->setLayout(cards_layout_layout);
        auto* cards_layout{ new WidgetWithLabel("Layout", cards_layout_container) };
        cards_layout->setEnabled(fit_size);
        cards_layout->setVisible(fit_size);

        auto* orientation{ new ComboBoxWithLabel{
            "&Orientation", std::array{ "Landscape"sv, "Portrait"sv }, project.Data.Orientation } };
        orientation->setEnabled(!fit_size && !infer_size);
        orientation->setVisible(!fit_size && !infer_size);

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
        layout->addWidget(print_output);
        layout->addWidget(paper_size);
        layout->addWidget(base_pdf);
        layout->addWidget(cards_layout);
        layout->addWidget(orientation);
        layout->addWidget(enable_guides_checkbox);
        layout->addWidget(extended_guides_checkbox);
        layout->addWidget(guides_color_a);
        layout->addWidget(guides_color_b);
        setLayout(layout);

        auto main_window{
            [this]()
            { return static_cast<PrintProxyPrepMainWindow*>(window()); }
        };

        auto change_output{
            [=, &project](QString t)
            {
                project.Data.FileName = t.toStdString();
            }
        };

        auto change_papersize{
            [=, &project](QString t)
            {
                project.Data.PageSize = t.toStdString();

                const bool fit_size{ project.Data.PageSize == Config::FitSize };
                const bool infer_size{ project.Data.PageSize == Config::BasePDFSize };
                const Size card_size_with_bleed{ CardSizeWithoutBleed + 2 * project.Data.BleedEdge };

                if (fit_size)
                {
                    project.Data.PageSizePhysical = card_size_with_bleed * dla::vec2{ project.Data.CardLayout };
                }
                else
                {
                    if (infer_size)
                    {
                        project.Data.PageSizePhysical = LoadPdfSize(project.Data.BasePdf + ".pdf")
                                                            .value_or(CFG.PageSizes["A4"].Dimensions);
                        base_pdf_info->GetWidget()->setText(ToQString(PageSizeToString(project.Data.PageSizePhysical)));

                        const auto max_margins{ project.Data.PageSizePhysical - card_size_with_bleed * project.Data.CardLayout };
                        base_pdf_top_margin_spin->setRange(0, max_margins.x / 1_mm);
                        base_pdf_top_margin_spin->setValue(max_margins.x / 2_mm);
                        base_pdf_left_margin_spin->setRange(0, max_margins.y / 1_mm);
                        base_pdf_left_margin_spin->setValue(max_margins.y / 2_mm);
                    }
                    else
                    {
                        project.Data.PageSizePhysical = CFG.PageSizes[project.Data.PageSize].Dimensions;
                    }
                    project.Data.CardLayout = static_cast<dla::uvec2>(dla::floor(project.Data.PageSizePhysical / card_size_with_bleed));
                }

                base_pdf->setEnabled(infer_size);
                base_pdf->setVisible(infer_size);
                cards_layout->setEnabled(fit_size);
                cards_layout->setVisible(fit_size);
                orientation->setEnabled(!fit_size && !infer_size);
                orientation->setVisible(!fit_size && !infer_size);

                main_window()->PageSizeChanged(project);
            }
        };

        auto change_base_pdf{
            [=, &project](QString t)
            {
                project.Data.BasePdf = t.toStdString();

                const bool infer_size{ project.Data.PageSize == Config::BasePDFSize };
                if (infer_size)
                {
                    project.Data.PageSizePhysical = LoadPdfSize(project.Data.BasePdf + ".pdf")
                                                        .value_or(CFG.PageSizes["A4"].Dimensions);
                    base_pdf_info->GetWidget()->setText(ToQString(PageSizeToString(project.Data.PageSizePhysical)));

                    const Size card_size_with_bleed{ CardSizeWithoutBleed + 2 * project.Data.BleedEdge };
                    project.Data.CardLayout = static_cast<dla::uvec2>(dla::floor(project.Data.PageSizePhysical / card_size_with_bleed));

                    const auto max_margins{ project.Data.PageSizePhysical - card_size_with_bleed * project.Data.CardLayout };
                    base_pdf_top_margin_spin->setRange(0, max_margins.x / 1_mm);
                    base_pdf_top_margin_spin->setValue(max_margins.x / 2_mm);
                    base_pdf_left_margin_spin->setRange(0, max_margins.y / 1_mm);
                    base_pdf_left_margin_spin->setValue(max_margins.y / 2_mm);
                }

                main_window()->PageSizeChanged(project);
            }
        };

        auto change_top_margin{
            [=, &project](double v)
            {
                project.Data.CustomMargins.y = static_cast<float>(v) * 1_mm;
                main_window()->CustomMarginsChanged(project);
            }
        };

        auto change_left_margin{
            [=, &project](double v)
            {
                project.Data.CustomMargins.x = static_cast<float>(v) * 1_mm;
                main_window()->CustomMarginsChanged(project);
            }
        };

        auto change_cards_width{
            [=, &project](double v)
            {
                project.Data.CardLayout.x = static_cast<uint32_t>(v);
                main_window()->CardLayoutChanged(project);
            }
        };

        auto change_cards_height{
            [=, &project](double v)
            {
                project.Data.CardLayout.y = static_cast<uint32_t>(v);
                main_window()->CardLayoutChanged(project);
            }
        };

        auto change_orientation{
            [=, &project](QString t)
            {
                project.Data.Orientation = t.toStdString();
                main_window()->OrientationChanged(project);
            }
        };

        auto change_enable_guides{
            [=, &project](Qt::CheckState s)
            {
                const bool enabled{ s == Qt::CheckState::Checked };
                project.Data.EnableGuides = enabled;

                extended_guides_checkbox->setEnabled(enabled);
                guides_color_a->setEnabled(enabled);
                guides_color_b->setEnabled(enabled);

                main_window()->GuidesEnabledChanged(project);
            }
        };

        auto change_extended_guides{
            [=, &project](Qt::CheckState s)
            {
                project.Data.ExtendedGuides = s == Qt::CheckState::Checked;
            }
        };

        auto pick_color{
            [&project](const ColorRGB8& color) -> std::optional<ColorRGB8>
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
            [=, this, &project]()
            {
                if (const auto picked_color{ pick_color(project.Data.GuidesColorA) })
                {
                    project.Data.GuidesColorA = picked_color.value();
                    guides_color_a_button->setStyleSheet(color_to_bg_style(project.Data.GuidesColorA));
                    main_window()->GuidesColorChanged(project);
                }
            }
        };

        auto pick_color_b{
            [=, &project]()
            {
                if (const auto picked_color{ pick_color(project.Data.GuidesColorB) })
                {
                    project.Data.GuidesColorB = picked_color.value();
                    guides_color_b_button->setStyleSheet(color_to_bg_style(project.Data.GuidesColorB));
                    main_window()->GuidesColorChanged(project);
                }
            }
        };

        QObject::connect(print_output->GetWidget(),
                         &QLineEdit::textChanged,
                         this,
                         change_output);
        QObject::connect(paper_size->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_papersize);
        QObject::connect(base_pdf_choice->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_base_pdf);
        QObject::connect(base_pdf_top_margin_spin,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_top_margin);
        QObject::connect(base_pdf_left_margin_spin,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_left_margin);
        QObject::connect(cards_width,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_cards_width);
        QObject::connect(cards_height,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_cards_height);
        QObject::connect(orientation->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_orientation);
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

        PrintOutput = print_output->GetWidget();
        PaperSize = paper_size->GetWidget();
        Orientation = orientation->GetWidget();
        EnableGuides = enable_guides_checkbox;
        ExtendedGuides = extended_guides_checkbox;
        GuidesColorA = guides_color_a;
        GuidesColorB = guides_color_b;
        BasePdf = base_pdf;
    }

    void RefreshWidgets(const Project& project)
    {
        const int base_pdf_size_idx{
            [&]()
            {
                for (int i = 0; i < PaperSize->count(); i++)
                {
                    if (PaperSize->itemData(i).toString().toStdString() == Config::BasePDFSize)
                    {
                        return i;
                    }
                }
                return -1;
            }()
        };
        const bool has_base_pdf_option{ base_pdf_size_idx >= 0 };
        if (!has_base_pdf_option && project.Data.PageSize.contains(Config::BasePDFSize))
        {
            PaperSize->addItem(ToQString(Config::BasePDFSize));
        }
        else if (has_base_pdf_option && !project.Data.PageSize.contains(Config::BasePDFSize))
        {
            PaperSize->removeItem(base_pdf_size_idx);
        }
        PaperSize->setCurrentText(ToQString(project.Data.PageSize));

        PrintOutput->setText(ToQString(project.Data.FileName.c_str()));
        Orientation->setCurrentText(ToQString(project.Data.Orientation));
        EnableGuides->setChecked(project.Data.EnableGuides);

        ExtendedGuides->setEnabled(project.Data.EnableGuides);
        GuidesColorA->setEnabled(project.Data.EnableGuides);
        GuidesColorB->setEnabled(project.Data.EnableGuides);

        const bool infer_size{ project.Data.PageSize == Config::BasePDFSize };
        BasePdf->setEnabled(infer_size);
        BasePdf->setVisible(infer_size);
    }

  private:
    static std::vector<std::string> GetBasePdfNames()
    {
        std::vector<std::string> base_pdf_names{ "Empty A4" };

        QDirIterator it("./res/base_pdfs");
        while (it.hasNext())
        {
            const QFileInfo next{ it.nextFileInfo() };
            if (!next.isFile() || next.suffix().toLower() != "pdf")
            {
                continue;
            }

            std::string base_name{ next.baseName().toStdString() };
            if (std::ranges::contains(base_pdf_names, base_name))
            {
                continue;
            }

            base_pdf_names.push_back(std::move(base_name));
        }

        return base_pdf_names;
    }

    static std::string PageSizeToString(Size page_size)
    {
        return fmt::format("{:.2}cm x {:.2}cm", page_size.x / 1_cm, page_size.y / 1_cm);
    }

    QLineEdit* PrintOutput;
    QComboBox* PaperSize;
    QComboBox* Orientation;
    QCheckBox* EnableGuides;
    QCheckBox* ExtendedGuides;
    WidgetWithLabel* GuidesColorA;
    WidgetWithLabel* GuidesColorB;
    QWidget* BasePdf;
};

class DefaultBacksidePreview : public QWidget
{
  public:
    DefaultBacksidePreview(const Project& project)
    {
        const fs::path& backside_name{ project.Data.BacksideDefault };

        auto* backside_default_image{ new BacksideImage{ backside_name, MinimumWidth, project } };

        const auto backside_height{ backside_default_image->heightForWidth(MinimumWidth.value) };
        backside_default_image->setFixedWidth(MinimumWidth.value);
        backside_default_image->setFixedHeight(backside_height);

        auto* backside_default_label{ new QLabel{ ToQString(backside_name.c_str()) } };

        auto* layout{ new QVBoxLayout };
        layout->addWidget(backside_default_image);
        layout->addWidget(backside_default_label);
        layout->setAlignment(backside_default_image, Qt::AlignmentFlag::AlignHCenter);
        layout->setAlignment(backside_default_label, Qt::AlignmentFlag::AlignHCenter);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        setLayout(layout);

        setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);

        DefaultImage = backside_default_image;
        DefaultLabel = backside_default_label;
    }

    void Refresh(const Project& project)
    {
        const fs::path& backside_name{ project.Data.BacksideDefault };
        DefaultImage->Refresh(backside_name, MinimumWidth, project);
        DefaultLabel->setText(ToQString(backside_name.c_str()));
    }

  private:
    inline static constexpr auto MinimumWidth{ 60_pix };

    BacksideImage* DefaultImage;
    QLabel* DefaultLabel;
};

class CardOptionsWidget : public QGroupBox
{
  public:
    CardOptionsWidget(Project& project)
    {
        setTitle("Card Options");

        auto* bleed_edge{ new DoubleSpinBoxWithLabel{ "&Bleed Edge" } };
        auto* bleed_edge_spin{ bleed_edge->GetWidget() };
        bleed_edge_spin->setDecimals(2);
        bleed_edge_spin->setRange(0, 0.12_in / 1_mm);
        bleed_edge_spin->setSingleStep(0.1);
        bleed_edge_spin->setSuffix("mm");
        bleed_edge_spin->setValue(project.Data.BleedEdge / 1_mm);

        auto* corner_weight_slider{ new QSlider{ Qt::Horizontal } };
        corner_weight_slider->setTickPosition(QSlider::NoTicks);
        corner_weight_slider->setRange(0, 1000);
        corner_weight_slider->setValue(static_cast<int>(project.Data.CornerWeight * 1000.0f));
        auto* corner_weight{ new WidgetWithLabel{ "&Corner Weight", corner_weight_slider } };

        auto* bleed_back_divider{ new QFrame };
        bleed_back_divider->setFrameShape(QFrame::Shape::HLine);
        bleed_back_divider->setFrameShadow(QFrame::Shadow::Sunken);

        auto* backside_checkbox{ new QCheckBox{ "Enable Backside" } };
        backside_checkbox->setChecked(project.Data.BacksideEnabled);

        auto* backside_guides_checkbox{ new QCheckBox{ "Enable Backside Guides" } };
        backside_guides_checkbox->setChecked(project.Data.BacksideEnableGuides);
        backside_guides_checkbox->setEnabled(project.Data.BacksideEnabled);
        backside_guides_checkbox->setVisible(project.Data.BacksideEnabled);

        auto* backside_default_button{ new QPushButton{ "Choose Default" } };
        backside_default_button->setEnabled(project.Data.BacksideEnabled);
        backside_default_button->setVisible(project.Data.BacksideEnabled);

        auto* backside_default_preview{ new DefaultBacksidePreview{ project } };
        backside_default_preview->setVisible(project.Data.BacksideEnabled);

        auto* backside_offset_spin{ new QDoubleSpinBox };
        backside_offset_spin->setDecimals(2);
        backside_offset_spin->setRange(-0.3_in / 1_mm, 0.3_in / 1_mm);
        backside_offset_spin->setSingleStep(0.1);
        backside_offset_spin->setSuffix("mm");
        backside_offset_spin->setValue(project.Data.BacksideOffset / 1_mm);
        auto* backside_offset{ new WidgetWithLabel{ "Off&set", backside_offset_spin } };
        backside_offset->setEnabled(project.Data.BacksideEnabled);
        backside_offset->setVisible(project.Data.BacksideEnabled);

        auto* back_over_divider{ new QFrame };
        back_over_divider->setFrameShape(QFrame::Shape::HLine);
        back_over_divider->setFrameShadow(QFrame::Shadow::Sunken);

        auto* oversized_checkbox{ new QCheckBox{ "Enable Oversized Option" } };
        oversized_checkbox->setChecked(project.Data.OversizedEnabled);

        auto* layout{ new QVBoxLayout };
        layout->addWidget(bleed_edge);
        layout->addWidget(corner_weight);
        layout->addWidget(bleed_back_divider);
        layout->addWidget(backside_checkbox);
        layout->addWidget(backside_guides_checkbox);
        layout->addWidget(backside_default_button);
        layout->addWidget(backside_default_preview);
        layout->addWidget(backside_offset);
        layout->addWidget(back_over_divider);
        layout->addWidget(oversized_checkbox);

        layout->setAlignment(backside_default_preview, Qt::AlignmentFlag::AlignHCenter);

        setLayout(layout);

        auto main_window{
            [this]()
            { return static_cast<PrintProxyPrepMainWindow*>(window()); }
        };

        auto change_bleed_edge{
            [=, &project](double v)
            {
                project.Data.BleedEdge = 1_mm * static_cast<float>(v);
                main_window()->BleedChangedDiff(project.Data.BleedEdge);
                main_window()->BleedChanged(project);
            }
        };

        auto change_corner_weight{
            [=, &project](int v)
            {
                project.Data.CornerWeight = static_cast<float>(v) / 1000.0f;
                main_window()->CornerWeightChanged(project);
            }
        };

        auto switch_backside_enabled{
            [=, &project](Qt::CheckState s)
            {
                project.Data.BacksideEnabled = s == Qt::CheckState::Checked;
                backside_guides_checkbox->setEnabled(project.Data.BacksideEnabled);
                backside_guides_checkbox->setVisible(project.Data.BacksideEnabled);
                backside_default_button->setEnabled(project.Data.BacksideEnabled);
                backside_default_button->setVisible(project.Data.BacksideEnabled);
                backside_default_preview->setVisible(project.Data.BacksideEnabled);
                backside_offset->setEnabled(project.Data.BacksideEnabled);
                backside_offset->setVisible(project.Data.BacksideEnabled);
                main_window()->BacksideEnabledChanged(project);
            }
        };

        auto switch_backside_guides_enabled{
            [=, &project](Qt::CheckState s)
            {
                project.Data.BacksideEnableGuides = s == Qt::CheckState::Checked;
                main_window()->BacksideGuidesEnabledChanged(project);
            }
        };

        auto pick_backside{
            [=, &project]()
            {
                if (const auto default_backside_choice{ OpenImageDialog(project.Data.ImageDir) })
                {
                    project.Data.BacksideDefault = default_backside_choice.value();
                    main_window()->BacksideDefaultChanged(project);
                }
            }
        };

        auto change_backside_offset{
            [=, &project](double v)
            {
                project.Data.BacksideOffset = 1_mm * static_cast<float>(v);
                main_window()->BacksideOffsetChanged(project);
            }
        };

        auto switch_oversized_enabled{
            [=, &project](Qt::CheckState s)
            {
                project.Data.OversizedEnabled = s == Qt::CheckState::Checked;
                main_window()->OversizedEnabledChanged(project);
            }
        };

        QObject::connect(bleed_edge_spin,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_bleed_edge);
        QObject::connect(corner_weight_slider,
                         &QSlider::valueChanged,
                         this,
                         change_corner_weight);
        QObject::connect(backside_checkbox,
                         &QCheckBox::checkStateChanged,
                         this,
                         switch_backside_enabled);
        QObject::connect(backside_guides_checkbox,
                         &QCheckBox::checkStateChanged,
                         this,
                         switch_backside_guides_enabled);
        QObject::connect(backside_default_button,
                         &QPushButton::clicked,
                         this,
                         pick_backside);
        QObject::connect(backside_offset_spin,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_backside_offset);
        QObject::connect(oversized_checkbox,
                         &QCheckBox::checkStateChanged,
                         this,
                         switch_oversized_enabled);

        BleedEdgeSpin = bleed_edge_spin;
        BacksideCheckbox = backside_checkbox;
        BacksideOffsetSpin = backside_offset_spin;
        BacksideDefaultPreview = backside_default_preview;
        OversizedCheckbox = oversized_checkbox;
    }

    void Refresh(const Project& project)
    {
        BleedEdgeSpin->setValue(project.Data.BleedEdge / 1_mm);
        BacksideCheckbox->setChecked(project.Data.BacksideEnabled);
        BacksideOffsetSpin->setValue(project.Data.BacksideOffset.value);
        OversizedCheckbox->setChecked(project.Data.OversizedEnabled);
    }

    void RefreshWidgets(const Project& project)
    {
        BacksideDefaultPreview->Refresh(project);
    }

  private:
    QDoubleSpinBox* BleedEdgeSpin{ nullptr };
    QCheckBox* BacksideCheckbox{ nullptr };
    QDoubleSpinBox* BacksideOffsetSpin{ nullptr };
    DefaultBacksidePreview* BacksideDefaultPreview{ nullptr };
    QCheckBox* OversizedCheckbox{ nullptr };
};

class GlobalOptionsWidget : public QGroupBox
{
  public:
    GlobalOptionsWidget(PrintProxyPrepApplication& application, Project& project)
    {
        setTitle("Global Config");

        auto* display_columns_spin_box{ new QDoubleSpinBox };
        display_columns_spin_box->setDecimals(0);
        display_columns_spin_box->setRange(2, 10);
        display_columns_spin_box->setSingleStep(1);
        display_columns_spin_box->setValue(CFG.DisplayColumns);
        auto* display_columns{ new WidgetWithLabel{ "Display &Columns", display_columns_spin_box } };
        display_columns->setToolTip("Number columns in card view");

        auto* backend{ new ComboBoxWithLabel{
            "&Rendering Backend", magic_enum::enum_names<PdfBackend>(), magic_enum::enum_name(CFG.Backend) } };
        backend->GetWidget()->setToolTip("Determines how the backend used for rendering and the output format.");

        auto* precropped_checkbox{ new QCheckBox{ "Allow Precropped" } };
        precropped_checkbox->setChecked(CFG.EnableUncrop);
        precropped_checkbox->setToolTip("Allows putting pre-cropped images into images/crop");

        auto* color_cube{ new ComboBoxWithLabel{
            "Color C&ube", GetCubeNames(), CFG.ColorCube } };
        color_cube->GetWidget()->setToolTip("Requires rerunning cropper");

        auto* preview_width_spin_box{ new QDoubleSpinBox };
        preview_width_spin_box->setDecimals(0);
        preview_width_spin_box->setRange(120, 1000);
        preview_width_spin_box->setSingleStep(60);
        preview_width_spin_box->setSuffix("pixels");
        preview_width_spin_box->setValue(CFG.BasePreviewWidth / 1_pix);
        auto* preview_width{ new WidgetWithLabel{ "&Preview Width", preview_width_spin_box } };
        preview_width->setToolTip("Requires rerunning cropper to take effect");

        auto* max_dpi_spin_box{ new QDoubleSpinBox };
        max_dpi_spin_box->setDecimals(0);
        max_dpi_spin_box->setRange(300, 1200);
        max_dpi_spin_box->setSingleStep(100);
        max_dpi_spin_box->setValue(CFG.MaxDPI / 1_dpi);
        auto* max_dpi{ new WidgetWithLabel{ "&Max DPI", max_dpi_spin_box } };
        max_dpi->setToolTip("Requires rerunning cropper");

        auto* paper_sizes{ new ComboBoxWithLabel{
            "Default P&aper Size", std::views::keys(CFG.PageSizes) | std::ranges::to<std::vector>(), CFG.DefaultPageSize } };

        auto* themes{ new ComboBoxWithLabel{
            "&Theme", GetStyles(), application.GetTheme() } };

        auto* layout{ new QVBoxLayout };
        layout->addWidget(display_columns);
        layout->addWidget(backend);
        layout->addWidget(precropped_checkbox);
        layout->addWidget(color_cube);
        layout->addWidget(preview_width);
        layout->addWidget(max_dpi);
        layout->addWidget(paper_sizes);
        layout->addWidget(themes);
        setLayout(layout);

        auto main_window{
            [this]()
            { return static_cast<PrintProxyPrepMainWindow*>(window()); }
        };

        auto change_display_columns{
            [=, &project](double v)
            {
                CFG.DisplayColumns = static_cast<int>(v);
                SaveConfig(CFG);
                main_window()->DisplayColumnsChanged(project);
            }
        };

        auto change_render_backend{
            [=, &application, &project](const QString& t)
            {
                CFG.SetPdfBackend(magic_enum::enum_cast<PdfBackend>(t.toStdString())
                                      .value_or(PdfBackend::LibHaru));
                if (CFG.Backend != PdfBackend::PoDoFo && project.Data.PageSize == Config::BasePDFSize)
                {
                    project.Data.PageSize = CFG.DefaultPageSize;
                    main_window()->PageSizeChanged(project);
                }
                SaveConfig(CFG);
                main_window()->RenderBackendChanged(project);
            }
        };

        auto change_precropped{
            [=, &project](Qt::CheckState s)
            {
                CFG.EnableUncrop = s == Qt::CheckState::Checked;
                SaveConfig(CFG);
                main_window()->EnableUncropChangedDiff(CFG.EnableUncrop);
                main_window()->EnableUncropChanged(project);
            }
        };

        auto change_color_cube{
            [=, &application, &project](const QString& t)
            {
                CFG.ColorCube = t.toStdString();
                SaveConfig(CFG);
                PreloadCube(application, CFG.ColorCube);
                main_window()->ColorCubeChangedDiff(CFG.ColorCube);
                main_window()->ColorCubeChanged(project);
            }
        };

        auto change_preview_width{
            [=, &project](double v)
            {
                CFG.BasePreviewWidth = static_cast<float>(v) * 1_pix;
                SaveConfig(CFG);
                main_window()->BasePreviewWidthChangedDiff(CFG.BasePreviewWidth);
                main_window()->BasePreviewWidthChanged(project);
            }
        };

        auto change_max_dpi{
            [=, &project](double v)
            {
                CFG.MaxDPI = static_cast<float>(v) * 1_dpi;
                SaveConfig(CFG);
                main_window()->MaxDPIChangedDiff(CFG.MaxDPI);
                main_window()->MaxDPIChanged(project);
            }
        };

        auto change_papersize{
            [=](const QString& t)
            {
                CFG.DefaultPageSize = t.toStdString();
                SaveConfig(CFG);
            }
        };

        auto change_theme{
            [=, &application](const QString& t)
            {
                application.SetTheme(t.toStdString());
                SetStyle(application, application.GetTheme());
            }
        };

        QObject::connect(display_columns_spin_box,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_display_columns);
        QObject::connect(backend->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_render_backend);
        QObject::connect(precropped_checkbox,
                         &QCheckBox::checkStateChanged,
                         this,
                         change_precropped);
        QObject::connect(color_cube->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_color_cube);
        QObject::connect(preview_width_spin_box,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_preview_width);
        QObject::connect(max_dpi_spin_box,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_max_dpi);
        QObject::connect(paper_sizes->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_papersize);
        QObject::connect(themes->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_theme);
    }
};

OptionsWidget::OptionsWidget(PrintProxyPrepApplication& application, Project& project)
{
    auto* actions_widget{ new ActionsWidget{ application, project } };
    auto* print_options{ new PrintOptionsWidget{ project } };
    auto* card_options{ new CardOptionsWidget{ project } };
    auto* global_options{ new GlobalOptionsWidget{ application, project } };

    auto* widget{ new QWidget };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(actions_widget);
    layout->addWidget(print_options);
    layout->addWidget(card_options);
    layout->addWidget(global_options);
    layout->addStretch();

    widget->setLayout(layout);
    widget->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::MinimumExpanding);

    setWidget(widget);
    setMinimumHeight(400);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);

    Actions = actions_widget;
    PrintOptions = print_options;
    CardOptions = card_options;
}

void OptionsWidget::Refresh(const Project& project)
{
    CardOptions->Refresh(project);
    widget()->adjustSize();
}

void OptionsWidget::RefreshWidgets(const Project& project)
{
    PrintOptions->RefreshWidgets(project);
    CardOptions->RefreshWidgets(project);
}

void OptionsWidget::CropperWorking()
{
    Actions->CropperWorking();
}

void OptionsWidget::CropperDone()
{
    Actions->CropperDone();
}

void OptionsWidget::CropperProgress(float progress)
{
    Actions->CropperProgress(progress);
}
