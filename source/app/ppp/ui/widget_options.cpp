#include <ppp/ui/widget_options.hpp>

#include <atomic>
#include <cstdint>
#include <ranges>

#include <QCheckBox>
#include <QColorDialog>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSlider>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <ppp/app.hpp>
#include <ppp/cubes.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/style.hpp>

#include <ppp/pdf/generate.hpp>

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

        auto* cropper_button{ new QPushButton{ "Run Cropper" } };
        auto* render_button{ new QPushButton{ "Render Document" } };
        auto* save_button{ new QPushButton{ "Save Project" } };
        auto* load_button{ new QPushButton{ "Load Project" } };
        auto* set_images_button{ new QPushButton{ "Set Image Folder" } };
        auto* open_images_button{ new QPushButton{ "Open Images" } };

        const QWidget* buttons[]{
            cropper_button,
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
        layout->addWidget(cropper_button, 0, 0);
        layout->addWidget(render_button, 0, 1);
        layout->addWidget(save_button, 1, 0);
        layout->addWidget(load_button, 1, 1);
        layout->addWidget(set_images_button, 2, 0);
        layout->addWidget(open_images_button, 2, 1);
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
                            for (const auto& [img, info] : project.Cards)
                            {
                                if (info.Num > 0)
                                {
                                    used_cards.push_back(img);
                                    if (project.BacksideEnabled && !std::ranges::contains(used_cards, info.Backside))
                                    {
                                        used_cards.push_back(info.Backside.empty() ? project.BacksideDefault : info.Backside);
                                    }
                                }
                            }

                            const Length bleed_edge{ project.BleedEdge };
                            const fs::path& image_dir{ project.ImageDir };
                            const fs::path& crop_dir{ project.CropDir };
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

        const auto run_cropper{
            [=, this, &application, &project]()
            {
                const Length bleed_edge{ project.BleedEdge };
                const fs::path& image_dir{ project.ImageDir };
                const fs::path& crop_dir{ project.CropDir };
                const bool need_cache_previews{ NeedCachePreviews(crop_dir, project.Previews) };
                if (NeedRunCropper(image_dir, crop_dir, bleed_edge, CFG.ColorCube) || need_cache_previews)
                {
                    std::atomic_bool data_available{ false };
                    bool rebuild_after_cropper{ need_cache_previews };
                    auto cards{ project.Cards };
                    auto previews{ project.Previews };
                    const auto img_cache{ project.ImageCache };

                    {
                        GenericPopup crop_window{ window(), "Cropping images..." };
                        const auto cropper_work{
                            [=, &application, &data_available, &rebuild_after_cropper, &cards, &previews, &img_cache, &crop_window]()
                            {
                                const fs::path& image_cache{ img_cache };
                                const auto print_fn{ crop_window.MakePrintFn() };
                                previews = RunCropper(image_dir, crop_dir, image_cache, previews, bleed_edge, CFG.MaxDPI, CFG.ColorCube, GetCubeImage(application, CFG.ColorCube), CFG.EnableUncrop, print_fn);
                                for (const auto& img : ListImageFiles(crop_dir))
                                {
                                    if (!cards.contains(img))
                                    {
                                        cards[img] = CardInfo{};
                                        if (img.string().starts_with("__"))
                                        {
                                            cards[img].Num = 0;
                                        }
                                        rebuild_after_cropper = true;
                                    }
                                }

                                for (auto it = cards.begin(); it != cards.end();)
                                {
                                    const auto& [img, _]{ *it };
                                    if (!previews.contains(img))
                                    {
                                        it = cards.erase(it);
                                        rebuild_after_cropper = true;
                                    }
                                    else
                                    {
                                        ++it;
                                    }
                                }

                                data_available.store(true, std::memory_order::release);
                            }
                        };

                        window()->setEnabled(false);
                        crop_window.ShowDuringWork(cropper_work);
                    }

                    while (!data_available.load(std::memory_order::acquire))
                    {
                        /*spin*/
                    }

                    std::swap(cards, project.Cards);
                    std::swap(previews, project.Previews);

                    auto main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
                    if (rebuild_after_cropper)
                    {
                        main_window->Refresh();
                    }
                    else
                    {
                        main_window->RefreshPreview();
                    }
                    window()->setEnabled(true);
                }
                else
                {
                    QToolTip::showText(QCursor::pos(), "All images are already cropped");
                }
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
                    application.SetProjectPath(new_project_json.value());
                    GenericPopup reload_window{ window(), "Reloading project..." };

                    const auto load_project_work{
                        [=, &project, &application, &reload_window]()
                        {
                            project.Load(new_project_json.value(), GetCubeImage(application, CFG.ColorCube), reload_window.MakePrintFn());
                        }
                    };

                    auto main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };

                    main_window->setEnabled(false);
                    reload_window.ShowDuringWork(load_project_work);
                    main_window->RefreshWidgets();
                    main_window->Refresh();
                    main_window->setEnabled(true);
                }
            }
        };

        const auto set_images_folder{
            [=, this, &project, &application]()
            {
                if (const auto new_image_dir{ OpenFolderDialog(".") })
                {
                    project.ImageDir = new_image_dir.value();
                    project.ImageCache = project.ImageDir.filename().replace_extension(".cache");

                    project.InitProperties(nullptr);

                    auto main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };

                    const Length bleed_edge{ project.BleedEdge };
                    const fs::path& image_dir{ project.ImageDir };
                    const fs::path& crop_dir{ project.CropDir };
                    if (NeedRunCropper(image_dir, crop_dir, bleed_edge, CFG.ColorCube) || NeedCachePreviews(crop_dir, project.Previews))
                    {
                        GenericPopup reload_window{ window(), "Reloading project..." };

                        const auto reload_work{
                            [=, &project, &application, &reload_window]()
                            {
                                project.InitImages(GetCubeImage(application, CFG.ColorCube), reload_window.MakePrintFn());
                            }
                        };

                        main_window->setEnabled(false);
                        reload_window.ShowDuringWork(reload_work);
                        main_window->Refresh();
                        main_window->setEnabled(true);
                    }
                    else
                    {
                        main_window->Refresh();
                    }
                }
            }
        };

        const auto open_images_folder{
            [=, &project]()
            {
                OpenFolder(project.ImageDir);
            }
        };

        QObject::connect(render_button,
                         &QPushButton::clicked,
                         this,
                         render);
        QObject::connect(cropper_button,
                         &QPushButton::clicked,
                         this,
                         run_cropper);
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
    }
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

        using namespace std::string_view_literals;
        auto* print_output{ new LineEditWithLabel{ "PDF &Filename", project.FileName.string() } };
        auto* paper_size{ new ComboBoxWithLabel{
            "&Paper Size", std::views::keys(CFG.PageSizes) | std::ranges::to<std::vector>(), project.PageSize } };
        auto* orientation{ new ComboBoxWithLabel{
            "&Orientation", std::array{ "Landscape"sv, "Portrait"sv }, project.Orientation } };
        auto* enable_guides_checkbox{ new QCheckBox{ "Enable Guides" } };
        enable_guides_checkbox->setChecked(project.EnableGuides);
        auto* extended_guides_checkbox{ new QCheckBox{ "Extended Guides" } };
        extended_guides_checkbox->setChecked(project.ExtendedGuides);
        extended_guides_checkbox->setEnabled(project.EnableGuides);
        auto* guides_color_a_button{ new QPushButton };
        guides_color_a_button->setStyleSheet(color_to_bg_style(project.GuidesColorA));
        auto* guides_color_a{ new WidgetWithLabel{ "Guides Color A", guides_color_a_button } };
        guides_color_a->setEnabled(project.EnableGuides);
        auto* guides_color_b_button{ new QPushButton };
        guides_color_b_button->setStyleSheet(color_to_bg_style(project.GuidesColorB));
        auto* guides_color_b{ new WidgetWithLabel{ "Guides Color B", guides_color_b_button } };
        guides_color_b->setEnabled(project.EnableGuides);

        auto* layout{ new QVBoxLayout };
        layout->addWidget(print_output);
        layout->addWidget(paper_size);
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
                project.FileName = t.toStdString();
            }
        };

        auto change_papersize{
            [=, &project](QString t)
            {
                project.PageSize = t.toStdString();
                main_window()->RefreshPreview();
            }
        };

        auto change_orientation{
            [=, &project](QString t)
            {
                project.Orientation = t.toStdString();
                main_window()->RefreshPreview();
            }
        };

        auto change_enable_guides{
            [=, &project](Qt::CheckState s)
            {
                const bool enabled{ s == Qt::CheckState::Checked };
                project.EnableGuides = enabled;

                extended_guides_checkbox->setEnabled(enabled);
                guides_color_a->setEnabled(enabled);
                guides_color_b->setEnabled(enabled);

                main_window()->RefreshPreview();
            }
        };

        auto change_extended_guides{
            [=, &project](Qt::CheckState s)
            {
                project.ExtendedGuides = s == Qt::CheckState::Checked;
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
                if (const auto picked_color{ pick_color(project.GuidesColorA) })
                {
                    project.GuidesColorA = picked_color.value();
                    guides_color_a_button->setStyleSheet(color_to_bg_style(project.GuidesColorA));
                    main_window()->RefreshPreview();
                }
            }
        };

        auto pick_color_b{
            [=, &project]()
            {
                if (const auto picked_color{ pick_color(project.GuidesColorB) })
                {
                    project.GuidesColorB = picked_color.value();
                    guides_color_b_button->setStyleSheet(color_to_bg_style(project.GuidesColorB));
                    main_window()->RefreshPreview();
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
    }

    void RefreshWidgets(const Project& project)
    {
        PrintOutput->setText(ToQString(project.FileName.c_str()));
        PaperSize->setCurrentText(ToQString(project.PageSize));
        Orientation->setCurrentText(ToQString(project.Orientation));
        EnableGuides->setChecked(project.EnableGuides);

        ExtendedGuides->setEnabled(project.EnableGuides);
        GuidesColorA->setEnabled(project.EnableGuides);
        GuidesColorB->setEnabled(project.EnableGuides);
    }

  private:
    QLineEdit* PrintOutput;
    QComboBox* PaperSize;
    QComboBox* Orientation;
    QCheckBox* EnableGuides;
    QCheckBox* ExtendedGuides;
    WidgetWithLabel* GuidesColorA;
    WidgetWithLabel* GuidesColorB;
};

class DefaultBacksidePreview : public QWidget
{
  public:
    DefaultBacksidePreview(const Project& project)
    {
        const fs::path& backside_name{ project.BacksideDefault };

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
        const fs::path& backside_name{ project.BacksideDefault };
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

        auto* bleed_edge_spin{ new QDoubleSpinBox };
        bleed_edge_spin->setDecimals(2);
        bleed_edge_spin->setRange(0, 0.12_in / 1_mm);
        bleed_edge_spin->setSingleStep(0.1);
        bleed_edge_spin->setSuffix("mm");
        bleed_edge_spin->setValue(project.BleedEdge / 1_mm);
        auto* bleed_edge{ new WidgetWithLabel{ "&Bleed Edge", bleed_edge_spin } };

        auto* corner_weight_slider{ new QSlider{ Qt::Horizontal } };
        corner_weight_slider->setTickPosition(QSlider::NoTicks);
        corner_weight_slider->setRange(0, 1000);
        corner_weight_slider->setValue(static_cast<int>(project.CornerWeight * 1000.0f));
        auto* corner_weight{ new WidgetWithLabel{ "&Corner Weight", corner_weight_slider } };

        auto* bleed_back_divider{ new QFrame };
        bleed_back_divider->setFrameShape(QFrame::Shape::HLine);
        bleed_back_divider->setFrameShadow(QFrame::Shadow::Sunken);

        auto* backside_checkbox{ new QCheckBox{ "Enable Backside" } };
        backside_checkbox->setChecked(project.BacksideEnabled);

        auto* backside_default_button{ new QPushButton{ "Choose Default" } };
        backside_default_button->setEnabled(project.BacksideEnabled);
        backside_default_button->setVisible(project.BacksideEnabled);

        auto* backside_default_preview{ new DefaultBacksidePreview{ project } };
        backside_default_preview->setVisible(project.BacksideEnabled);

        auto* backside_offset_spin{ new QDoubleSpinBox };
        backside_offset_spin->setDecimals(2);
        backside_offset_spin->setRange(-0.3_in / 1_mm, 0.3_in / 1_mm);
        backside_offset_spin->setSingleStep(0.1);
        backside_offset_spin->setSuffix("mm");
        backside_offset_spin->setValue(project.BacksideOffset / 1_mm);
        auto* backside_offset{ new WidgetWithLabel{ "Off&set", backside_offset_spin } };
        backside_offset->setEnabled(project.BacksideEnabled);
        backside_offset->setVisible(project.BacksideEnabled);

        auto* back_over_divider{ new QFrame };
        back_over_divider->setFrameShape(QFrame::Shape::HLine);
        back_over_divider->setFrameShadow(QFrame::Shadow::Sunken);

        auto* oversized_checkbox{ new QCheckBox{ "Enable Oversized Option" } };
        oversized_checkbox->setChecked(project.OversizedEnabled);

        auto* layout{ new QVBoxLayout };
        layout->addWidget(bleed_edge);
        layout->addWidget(corner_weight);
        layout->addWidget(bleed_back_divider);
        layout->addWidget(backside_checkbox);
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
                project.BleedEdge = 1_mm * static_cast<float>(v);
                main_window()->RefreshPreview();
            }
        };

        auto change_corner_weight{
            [=, &project](int v)
            {
                project.CornerWeight = static_cast<float>(v) / 1000.0f;
                main_window()->RefreshPreview();
            }
        };

        auto switch_backside_enabled{
            [=, &project](Qt::CheckState s)
            {
                project.BacksideEnabled = s == Qt::CheckState::Checked;
                backside_default_button->setEnabled(project.BacksideEnabled);
                backside_default_button->setVisible(project.BacksideEnabled);
                backside_default_preview->setVisible(project.BacksideEnabled);
                backside_offset->setEnabled(project.BacksideEnabled);
                backside_offset->setVisible(project.BacksideEnabled);
                main_window()->Refresh();
                main_window()->RefreshPreview();
            }
        };

        auto pick_backside{
            [=, &project]()
            {
                if (const auto default_backside_choice{ OpenImageDialog(project.ImageDir) })
                {
                    project.BacksideDefault = default_backside_choice.value();
                    main_window()->Refresh();
                }
            }
        };

        auto change_backside_offset{
            [=, &project](double v)
            {
                project.BacksideOffset = 1_mm * static_cast<float>(v);
                main_window()->RefreshPreview();
            }
        };

        auto switch_oversized_enabled{
            [=, &project](Qt::CheckState s)
            {
                project.OversizedEnabled = s == Qt::CheckState::Checked;
                main_window()->Refresh();
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
        BleedEdgeSpin->setValue(project.BleedEdge / 1_mm);
        BacksideCheckbox->setChecked(project.BacksideEnabled);
        BacksideOffsetSpin->setValue(project.BacksideOffset.value);
        OversizedCheckbox->setChecked(project.OversizedEnabled);
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
    GlobalOptionsWidget(PrintProxyPrepApplication& application)
    {
        setTitle("Global Config");

        auto* display_columns_spin_box{ new QDoubleSpinBox };
        display_columns_spin_box->setDecimals(0);
        display_columns_spin_box->setRange(2, 10);
        display_columns_spin_box->setSingleStep(1);
        display_columns_spin_box->setValue(CFG.DisplayColumns);
        auto* display_columns{ new WidgetWithLabel{ "Display &Columns", display_columns_spin_box } };
        display_columns->setToolTip("Number columns in card view");

        auto* precropped_checkbox{ new QCheckBox{ "Allow Precropped" } };
        precropped_checkbox->setChecked(CFG.EnableUncrop);
        precropped_checkbox->setToolTip("Allows putting pre-cropped images into images/crop");

        auto* color_cube{ new ComboBoxWithLabel{
            "Color C&ube", GetCubeNames(), "None" } };
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
            [=](double v)
            {
                CFG.DisplayColumns = static_cast<int>(v);
                SaveConfig(CFG);
                main_window()->Refresh();
            }
        };

        auto change_precropped{
            [=](Qt::CheckState s)
            {
                CFG.EnableUncrop = s == Qt::CheckState::Checked;
                SaveConfig(CFG);
            }
        };

        auto change_color_cube{
            [=, &application](const QString& t)
            {
                CFG.ColorCube = t.toStdString();
                PreloadCube(application, CFG.ColorCube);
            }
        };

        auto change_preview_width{
            [=](double v)
            {
                CFG.BasePreviewWidth = static_cast<float>(v) * 1_pix;
                SaveConfig(CFG);
            }
        };

        auto change_max_dpi{
            [=](double v)
            {
                CFG.MaxDPI = static_cast<float>(v) * 1_dpi;
                SaveConfig(CFG);
            }
        };

        auto change_papersize{
            [=](const QString& t)
            {
                CFG.DefaultPageSize = t.toStdString();
                SaveConfig(CFG);
                main_window()->RefreshPreview();
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
    auto* global_options{ new GlobalOptionsWidget{ application } };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(actions_widget);
    layout->addWidget(print_options);
    layout->addWidget(card_options);
    layout->addWidget(global_options);
    layout->addStretch();

    setLayout(layout);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);

    PrintOptions = print_options;
    CardOptions = card_options;
}

void OptionsWidget::Refresh(const Project& project)
{
    CardOptions->Refresh(project);
}

void OptionsWidget::RefreshWidgets(const Project& project)
{
    PrintOptions->RefreshWidgets(project);
    CardOptions->RefreshWidgets(project);
}
