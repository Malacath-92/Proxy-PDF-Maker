#include <ppp/ui/widget_options.hpp>

#include <atomic>
#include <cstdint>
#include <ranges>

#include <QCheckBox>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <ppp/app.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups.hpp>
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
            [&]()
            {
                const Length bleed_edge{ project.BleedEdge };
                const fs::path& image_dir{ project.ImageDir };
                const fs::path& crop_dir{ project.CropDir };
                if (NeedRunCropper(image_dir, crop_dir, bleed_edge, CFG.VibranceBump))
                {
                    QToolTip::showText(QCursor::pos(), "Cropper needs to be run first");
                    return;
                }

                const auto render_work{
                    [&]()
                    {
                        const fs::path pdf_path{ fs::path{ project.FileName }.replace_extension(".pdf") };
                        (void)pdf_path;
                        //  pages = pdf.generate(
                        //      print_dict,
                        //      crop_dir,
                        //      page_sizes[print_dict["pagesize"]],
                        //      pdf_path,
                        //      make_popup_print_fn(render_window),
                        //  )
                        //  make_popup_print_fn(render_window)("Saving PDF...")
                        //  pages.save()
                        //  try:
                        //      subprocess.Popen([pdf_path], shell=True)
                        //  except Exception as e:
                        //      print(e)
                    }
                };

                window()->setEnabled(false);
                auto* render_window{ new GenericPopup{ window(), "Rendering PDF..." } };
                render_window->ShowDuringWork(render_work);
                delete render_window;
                window()->setEnabled(true);
            }
        };

        const auto run_cropper{
            [&]()
            {
                const Length bleed_edge{ project.BleedEdge };
                const fs::path& image_dir{ project.ImageDir };
                const fs::path& crop_dir{ project.CropDir };
                if (NeedRunCropper(image_dir, crop_dir, bleed_edge, CFG.VibranceBump))
                {
                    std::atomic_bool rebuild_after_cropper{ false };
                    auto* crop_window{ new GenericPopup{ window(), "Cropping images..." } };

                    const auto cropper_work{
                        [&]()
                        {
                            const fs::path& image_cache{ project.ImageCache };
                            const auto print_fn{ crop_window->MakePrintFn() };
                            project.Previews = RunCropper(image_dir, crop_dir, image_cache, project.Previews, bleed_edge, CFG.MaxDPI, CFG.VibranceBump, CFG.EnableUncrop, print_fn);
                            for (const auto& img : ListImageFiles(crop_dir))
                            {
                                // TODO: Make project access thread-safe!!!
                                if (!project.Cards.contains(img))
                                {
                                    project.Cards[img] = CardInfo{};
                                    if (img.string().starts_with("__"))
                                    {
                                        project.Cards[img].Num = 0;
                                    }
                                    rebuild_after_cropper = true;
                                }
                            }

                            auto deleted_images{
                                project.Cards |
                                    std::views::transform([](const auto& item)
                                                          { return std::ref(item.first); }) |
                                    std::views::filter([&](const auto& img)
                                                       { return !project.Previews.contains(img); }),
                            };
                            for (const auto& img : deleted_images)
                            {
                                project.Cards.erase(img);
                                rebuild_after_cropper = !deleted_images.empty();
                            }
                        }
                    };

                    window()->setEnabled(false);
                    crop_window->ShowDuringWork(cropper_work);
                    delete crop_window;

                    auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
                    if (rebuild_after_cropper)
                    {
                        main_window->Refresh(project);
                    }
                    else
                    {
                        main_window->RefreshPreview(project);
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
            [&]()
            {
                if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Save) })
                {
                    application.SetProjectPath(new_project_json.value());
                    project.Dump(new_project_json.value(), nullptr);
                }
            }
        };

        const auto load_project{
            [&]()
            {
                if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Open) })
                {
                    application.SetProjectPath(new_project_json.value());
                    auto* reload_window{ new GenericPopup{ window(), "Reloading project..." } };

                    const auto load_project_work{
                        [&]()
                        {
                            project.Load(new_project_json.value(), reload_window->MakePrintFn());
                        }
                    };

                    auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };

                    main_window->setEnabled(false);
                    reload_window->ShowDuringWork(load_project_work);
                    delete reload_window;
                    main_window->RefreshWidgets(project);
                    main_window->Refresh(project);
                    main_window->setEnabled(true);
                }
            }
        };

        const auto set_images_folder{
            [&]()
            {
                if (const auto new_image_dir{ OpenFolderDialog(".") })
                {
                    project.ImageDir = new_image_dir.value();
                    if (project.ImageDir == "images")
                    {
                        project.ImageCache = "img.cache";
                    }
                    else
                    {
                        project.ImageCache = project.ImageDir.filename().replace_extension(".cache");
                    }

                    project.InitProperties(nullptr);

                    auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };

                    const Length bleed_edge{ project.BleedEdge };
                    const fs::path& image_dir{ project.ImageDir };
                    const fs::path& crop_dir{ project.CropDir };
                    if (NeedRunCropper(image_dir, crop_dir, bleed_edge, CFG.VibranceBump) || NeedCachePreviews(crop_dir, project.Previews))
                    {
                        auto* reload_window{ new GenericPopup{ window(), "Reloading project..." } };

                        const auto reload_work{
                            [&]()
                            {
                                project.InitImages(reload_window->MakePrintFn());
                            }
                        };

                        main_window->setEnabled(false);
                        reload_window->ShowDuringWork(reload_work);
                        delete reload_window;
                        main_window->Refresh(project);
                        main_window->setEnabled(true);
                    }
                    else
                    {
                        main_window->Refresh(project);
                    }
                }
            }
        };

        const auto open_images_folder{
            [&]()
            {
                // TODO
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

        using namespace std::string_view_literals;
        auto* print_output{ new LineEditWithLabel{ "PDF &Filename", project.FileName.string() } };
        auto* paper_size{ new ComboBoxWithLabel{
            "&Paper Size", std::views::keys(PageSizes) | std::ranges::to<std::vector>(), project.PageSize } };
        auto* orientation{ new ComboBoxWithLabel{
            "&Orientation", std::array{ "Landscape"sv, "Portrait"sv }, project.Orientation } };
        auto* guides_checkbox{ new QCheckBox{ "Extended Guides" } };
        guides_checkbox->setChecked(project.ExtendedGuides);

        auto* layout{ new QVBoxLayout };
        layout->addWidget(print_output);
        layout->addWidget(paper_size);
        layout->addWidget(orientation);
        layout->addWidget(guides_checkbox);
        setLayout(layout);

        auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };

        auto change_output{
            [&](QString t)
            {
                project.FileName = t.toStdString();
            }
        };

        auto change_papersize{
            [&](QString t)
            {
                project.PageSize = t.toStdString();
                main_window->RefreshPreview(project);
            }
        };

        auto change_orientation{
            [&](QString t)
            {
                project.Orientation = t.toStdString();
                main_window->RefreshPreview(project);
            }
        };

        auto change_guides{
            [&](Qt::CheckState s)
            {
                project.ExtendedGuides = s == Qt::CheckState::Checked;
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
        QObject::connect(guides_checkbox,
                         &QCheckBox::checkStateChanged,
                         this,
                         change_guides);

        PrintOutput = print_output->GetWidget();
        PaperSize = paper_size->GetWidget();
        Orientation = orientation->GetWidget();
        ExtendedGuides = guides_checkbox;
    }

    void RefreshWidgets(const Project& project)
    {
        PrintOutput->setText(QString::fromWCharArray(project.FileName.c_str()));
        PaperSize->setCurrentText(QString::fromStdString(project.PageSize));
        Orientation->setCurrentText(QString::fromStdString(project.Orientation));
        ExtendedGuides->setChecked(project.ExtendedGuides);
    }

  private:
    QLineEdit* PrintOutput;
    QComboBox* PaperSize;
    QComboBox* Orientation;
    QCheckBox* ExtendedGuides;
};

class BacksidePreview : public QWidget
{
  public:
};

class CardOptionsWidget : public QGroupBox
{
  public:
    CardOptionsWidget(const Project& /*project*/)
    {
    }

    void Refresh(const Project& project)
    {
        BleedEdgeSpin->setValue(project.BleedEdge.value);
        BacksideCheckbox->setChecked(project.BacksideEnabled);
        BacksideOffsetSpin->setValue(project.BacksideOffset.value);
        OversizedCheckbox->setChecked(project.OversizedEnabled);
    }
    void RefreshWidgets(const Project& /*project*/)
    {
        //BacksideDefaultPreview->Refresh(project);
    }

  private:
    QDoubleSpinBox* BleedEdgeSpin{ nullptr };
    QCheckBox* BacksideCheckbox{ nullptr };
    QDoubleSpinBox* BacksideOffsetSpin{ nullptr };
    BacksidePreview* BacksideDefaultPreview{ nullptr };
    QCheckBox* OversizedCheckbox{ nullptr };
};

class GlobalOptionsWidget : public QGroupBox
{
  public:
    GlobalOptionsWidget(const Project& /*project*/)
    {
    }
};

// class PrintOptionsWidget(QGroupBox):
//     def __init__(self, print_dict, img_dict):
//         super().__init__()
//
//         self.setTitle("Print Options")
//
//         print_output = LineEditWithLabel("PDF &Filename", print_dict["filename"])
//         paper_size = ComboBoxWithLabel(
//             "&Paper Size", list(page_sizes.keys()), print_dict["pagesize"]
//         )
//         orientation = ComboBoxWithLabel(
//             "&Orientation", ["Landscape", "Portrait"], print_dict["orient"]
//         )
//         guides_checkbox = QCheckBox("Extended Guides")
//         guides_checkbox.setChecked(print_dict["extended_guides"])
//
//         layout = QVBoxLayout()
//         layout.addWidget(print_output)
//         layout.addWidget(paper_size)
//         layout.addWidget(orientation)
//         layout.addWidget(guides_checkbox)
//
//         self.setLayout(layout)
//
//         def change_output(t):
//             print_dict["filename"] = t
//
//         def change_papersize(t):
//             print_dict["pagesize"] = t
//             self.window().refresh_preview(print_dict, img_dict)
//
//         def change_orientation(t):
//             print_dict["orient"] = t
//             self.window().refresh_preview(print_dict, img_dict)
//
//         def change_guides(s):
//             enabled = s == QtCore.Qt.CheckState.Checked
//             print_dict["extended_guides"] = enabled
//
//         print_output._widget.textChanged.connect(change_output)
//         paper_size._widget.currentTextChanged.connect(change_papersize)
//         orientation._widget.currentTextChanged.connect(change_orientation)
//         guides_checkbox.checkStateChanged.connect(change_guides)
//
//         self._print_output = print_output._widget
//         self._paper_size = paper_size._widget
//         self._orientation = orientation._widget
//         self._guides_checkbox = guides_checkbox

// class BacksidePreview(QWidget):
//     def __init__(self, backside_name, img_dict):
//         super().__init__()
//
//         self.setLayout(QVBoxLayout())
//         self.refresh(backside_name, img_dict)
//         self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
//
//     def refresh(self, backside_name, img_dict):
//         backside_default_image = BacksideImage(backside_name, img_dict)
//
//         backside_width = 120
//         backside_height = backside_default_image.heightForWidth(backside_width)
//         backside_default_image.setFixedWidth(backside_width)
//         backside_default_image.setFixedHeight(backside_height)
//
//         backside_default_label = QLabel(backside_name)
//
//         layout = self.layout()
//         for i in reversed(range(layout.count())):
//             layout.itemAt(i).widget().setParent(None)
//
//         layout.addWidget(backside_default_image)
//         layout.addWidget(backside_default_label)
//         layout.setAlignment(
//             backside_default_image, QtCore.Qt.AlignmentFlag.AlignHCenter
//         )
//         layout.setAlignment(
//             backside_default_label, QtCore.Qt.AlignmentFlag.AlignHCenter
//         )
//         layout.setSpacing(0)
//         layout.setContentsMargins(0, 0, 0, 0)
//
//         self.setLayout(layout)

// class CardOptionsWidget(QGroupBox):
//     def __init__(self, print_dict, img_dict):
//         super().__init__()
//
//         self.setTitle("Card Options")
//
//         bleed_edge_spin = QDoubleSpinBox()
//         bleed_edge_spin.setDecimals(2)
//         bleed_edge_spin.setRange(0, inch_to_mm(0.12))
//         bleed_edge_spin.setSingleStep(0.1)
//         bleed_edge_spin.setSuffix("mm")
//         bleed_edge_spin.setValue(float(print_dict["bleed_edge"]))
//         bleed_edge = WidgetWithLabel("&Bleed Edge", bleed_edge_spin)
//
//         bleed_back_divider = QFrame()
//         bleed_back_divider.setFrameShape(QFrame.Shape.HLine)
//         bleed_back_divider.setFrameShadow(QFrame.Shadow.Sunken)
//
//         backside_enabled = print_dict["backside_enabled"]
//         backside_checkbox = QCheckBox("Enable Backside")
//         backside_checkbox.setChecked(backside_enabled)
//
//         backside_default_button = QPushButton("Default")
//         backside_default_preview = BacksidePreview(
//             print_dict["backside_default"], img_dict
//         )
//
//         backside_offset_spin = QDoubleSpinBox()
//         backside_offset_spin.setDecimals(2)
//         backside_offset_spin.setRange(-inch_to_mm(0.3), inch_to_mm(0.3))
//         backside_offset_spin.setSingleStep(0.1)
//         backside_offset_spin.setSuffix("mm")
//         backside_offset_spin.setValue(float(print_dict["backside_offset"]))
//         backside_offset = WidgetWithLabel("Off&set", backside_offset_spin)
//
//         backside_default_button.setEnabled(backside_enabled)
//         backside_default_preview.setEnabled(backside_enabled)
//         backside_offset.setEnabled(backside_enabled)
//
//         back_over_divider = QFrame()
//         back_over_divider.setFrameShape(QFrame.Shape.HLine)
//         back_over_divider.setFrameShadow(QFrame.Shadow.Sunken)
//
//         oversized_enabled = print_dict["oversized_enabled"]
//         oversized_checkbox = QCheckBox("Enable Oversized Option")
//         oversized_checkbox.setChecked(oversized_enabled)
//
//         layout = QVBoxLayout()
//         layout.addWidget(bleed_edge)
//         layout.addWidget(bleed_back_divider)
//         layout.addWidget(backside_checkbox)
//         layout.addWidget(backside_default_button)
//         layout.addWidget(backside_default_preview)
//         layout.addWidget(backside_offset)
//         layout.addWidget(back_over_divider)
//         layout.addWidget(oversized_checkbox)
//
//         layout.setAlignment(
//             backside_default_preview, QtCore.Qt.AlignmentFlag.AlignHCenter
//         )
//
//         self.setLayout(layout)
//
//         def change_bleed_edge(v):
//             print_dict["bleed_edge"] = v
//             self.window().refresh_preview(print_dict, img_dict)
//
//         def switch_backside_enabled(s):
//             enabled = s == QtCore.Qt.CheckState.Checked
//             print_dict["backside_enabled"] = enabled
//             backside_default_button.setEnabled(enabled)
//             backside_default_preview.setEnabled(enabled)
//             self.window().refresh(print_dict, img_dict)
//
//         def pick_backside():
//             default_backside_choice = image_file_dialog(self, print_dict["image_dir"])
//             if default_backside_choice is not None:
//                 print_dict["backside_default"] = default_backside_choice
//                 backside_default_preview.refresh(
//                     print_dict["backside_default"], img_dict
//                 )
//                 self.window().refresh(print_dict, img_dict)
//
//         def change_backside_offset(v):
//             print_dict["backside_offset"] = v
//             self.window().refresh_preview(print_dict, img_dict)
//
//         def switch_oversized_enabled(s):
//             enabled = s == QtCore.Qt.CheckState.Checked
//             print_dict["oversized_enabled"] = enabled
//             self.window().refresh(print_dict, img_dict)
//
//         bleed_edge_spin.valueChanged.connect(change_bleed_edge)
//         backside_checkbox.checkStateChanged.connect(switch_backside_enabled)
//         backside_default_button.clicked.connect(pick_backside)
//         backside_offset_spin.valueChanged.connect(change_backside_offset)
//         oversized_checkbox.checkStateChanged.connect(switch_oversized_enabled)
//
//         self._bleed_edge_spin = bleed_edge_spin
//         self._backside_checkbox = backside_checkbox
//         self._backside_offset_spin = backside_offset_spin
//         self._backside_default_preview = backside_default_preview
//         self._oversized_checkbox = oversized_checkbox
//
//     def refresh_widgets(self, print_dict):
//         self._bleed_edge_spin.setValue(float(print_dict["bleed_edge"]))
//         self._backside_checkbox.setChecked(print_dict["backside_enabled"])
//         self._backside_offset_spin.setValue(float(print_dict["backside_offset"]))
//         self._oversized_checkbox.setChecked(print_dict["oversized_enabled"])
//
//     def refresh(self, print_dict, img_dict):
//         self._backside_default_preview.refresh(print_dict["backside_default"], img_dict)

// class GlobalOptionsWidget(QGroupBox):
//     def __init__(self, print_dict, img_dict):
//         super().__init__()
//
//         self.setTitle("Global Config")
//
//         display_columns_spin_box = QDoubleSpinBox()
//         display_columns_spin_box.setDecimals(0)
//         display_columns_spin_box.setRange(2, 10)
//         display_columns_spin_box.setSingleStep(1)
//         display_columns_spin_box.setValue(CFG.DisplayColumns)
//         display_columns = WidgetWithLabel("Display &Columns", display_columns_spin_box)
//         display_columns.setToolTip("Number columns in card view")
//
//         precropped_checkbox = QCheckBox("Allow Precropped")
//         precropped_checkbox.setChecked(CFG.EnableUncrop)
//         precropped_checkbox.setToolTip(
//             "Allows putting pre-cropped images into images/crop"
//         )
//
//         vibrance_checkbox = QCheckBox("Vibrance Bump")
//         vibrance_checkbox.setChecked(CFG.VibranceBump)
//         vibrance_checkbox.setToolTip("Requires rerunning cropper")
//
//         max_dpi_spin_box = QDoubleSpinBox()
//         max_dpi_spin_box.setDecimals(0)
//         max_dpi_spin_box.setRange(300, 1200)
//         max_dpi_spin_box.setSingleStep(100)
//         max_dpi_spin_box.setValue(CFG.MaxDPI)
//         max_dpi = WidgetWithLabel("&Max DPI", max_dpi_spin_box)
//         max_dpi.setToolTip("Requires rerunning cropper")
//
//         paper_sizes = ComboBoxWithLabel(
//             "Default P&aper Size", list(page_sizes.keys()), CFG.DefaultPageSize
//         )
//
//         layout = QVBoxLayout()
//         layout.addWidget(display_columns)
//         layout.addWidget(precropped_checkbox)
//         layout.addWidget(vibrance_checkbox)
//         layout.addWidget(max_dpi)
//         layout.addWidget(paper_sizes)
//
//         self.setLayout(layout)
//
//         def change_display_columns(v):
//             CFG.DisplayColumns = int(v)
//             save_config(CFG)
//             self.window().refresh(print_dict, img_dict)
//
//         def change_precropped(s):
//             enabled = s == QtCore.Qt.CheckState.Checked
//             CFG.EnableUncrop = enabled
//             save_config(CFG)
//
//         def change_vibrance_bump(s):
//             enabled = s == QtCore.Qt.CheckState.Checked
//             CFG.VibranceBump = enabled
//             save_config(CFG)
//             self.window().refresh_preview(print_dict, img_dict)
//
//         def change_max_dpi(v):
//             CFG.MaxDPI = int(v)
//             save_config(CFG)
//
//         def change_papersize(t):
//             CFG.DefaultPageSize = t
//             save_config(CFG)
//             self.window().refresh_preview(print_dict, img_dict)
//
//         display_columns_spin_box.valueChanged.connect(change_display_columns)
//         precropped_checkbox.checkStateChanged.connect(change_precropped)
//         vibrance_checkbox.checkStateChanged.connect(change_vibrance_bump)
//         max_dpi_spin_box.valueChanged.connect(change_max_dpi)
//         paper_sizes._widget.currentTextChanged.connect(change_papersize)

OptionsWidget::OptionsWidget(PrintProxyPrepApplication& application, Project& project)
{
    auto* actions_widget{ new ActionsWidget{ application, project } };
    auto* print_options{ new PrintOptionsWidget{ project } };
    auto* card_options{ new CardOptionsWidget{ project } };
    auto* global_options{ new GlobalOptionsWidget{ project } };

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
