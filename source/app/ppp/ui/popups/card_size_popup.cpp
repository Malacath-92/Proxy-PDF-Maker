#include <ppp/ui/popups/card_size_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <QCursor>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>
#include <ppp/units.hpp>

#include <ppp/ui/widget_combo_box.hpp>

CardSizePopup::CardSizePopup(QWidget* parent,
                             const Config& config)
    : PopupBase{ parent }
{
    m_AutoCenter = false;
    m_PersistGeometry = true;

    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("Edit Card Sizes");
    setObjectName("CardSizePopup");

    static constexpr auto c_MakeNumberEdit{
        [](QString initial_string)
        {
            auto* line_edit{ new QLineEdit{ std::move(initial_string) } };
            line_edit->setValidator(new QDoubleValidator{ -999, 999, 4 });
            return line_edit;
        }
    };

    const auto svg_files{
        []()
        {
            std::vector<std::string> svg_files{};

            QDirIterator it{ "./res/card_svgs" };
            while (it.hasNext())
            {
                const QFileInfo next{ it.nextFileInfo() };
                if (!next.isFile() || next.suffix().toLower() != "svg")
                {
                    continue;
                }

                std::string base_name{ next.baseName().toStdString() };
                if (std::ranges::contains(svg_files, base_name))
                {
                    continue;
                }

                svg_files.push_back(std::move(base_name));
            }

            return svg_files;
        }()
    };

    static constexpr auto make_svg_card_size_refresh{
        [](QLabel* card_size_label)
        {
            return [card_size_label](const QString& svg)
            {
                const auto loaded_svg{ LoadSvg("res/card_svgs/" + svg.toStdString() + ".svg") };
                const auto card_size{ loaded_svg.m_Size };
                const auto [card_width, card_height]{ (card_size / UnitValue(g_Cfg.m_BaseUnit)).pod() };
                const auto card_size_string{ QString{ "%1%3 x %2%3" }
                                                 .arg(card_width, 0, 'g', 2)
                                                 .arg(card_height, 0, 'g', 2)
                                                 .arg(ToQString(UnitShortName(g_Cfg.m_BaseUnit))) };
                card_size_label->setText(card_size_string);
            };
        }
    };

    auto build_tables{
        [this, svg_files](const auto& card_sizes)
        {
            m_RectTable->clearContents();
            m_RectTable->setRowCount(0);
            m_SvgTable->clearContents();
            m_SvgTable->setRowCount(0);

            QLocale locale{};
            for (const auto& [card_name, card_size_info] : card_sizes)
            {
                const auto& [bleed_size, bleed_base, bleed_decimals]{ card_size_info.m_InputBleed };
                const auto bleed_unit_value{ UnitValue(bleed_base) };
                const auto bleed_size_string{ locale.toString(bleed_size / bleed_unit_value, 'f', bleed_decimals) };

                if (card_size_info.m_RoundedRect.has_value())
                {
                    const auto& [card_size, card_base, card_decimals]{ card_size_info.m_RoundedRect->m_CardSize };
                    const auto card_unit_value{ UnitValue(card_base) };
                    const auto [card_width, card_height]{ (card_size / card_unit_value).pod() };
                    const auto card_width_string{ locale.toString(card_width, 'f', card_decimals) };
                    const auto card_height_string{ locale.toString(card_height, 'f', card_decimals) };

                    const auto& [corner_size, corner_base, corner_decimals]{ card_size_info.m_RoundedRect->m_CornerRadius };
                    const auto corner_unit_value{ UnitValue(corner_base) };
                    const auto corner_size_string{ locale.toString(corner_size / corner_unit_value, 'f', corner_decimals) };

                    int i{ m_RectTable->rowCount() };
                    m_RectTable->insertRow(i);
                    m_RectTable->setCellWidget(i, 0, new QLineEdit{ ToQString(card_name) });

                    // card size
                    m_RectTable->setCellWidget(i, 1, c_MakeNumberEdit(card_width_string));
                    m_RectTable->setCellWidget(i, 2, c_MakeNumberEdit(card_height_string));
                    m_RectTable->setCellWidget(i, 3, MakeComboBox(card_base));

                    // input bleed
                    m_RectTable->setCellWidget(i, 4, c_MakeNumberEdit(bleed_size_string));
                    m_RectTable->setCellWidget(i, 5, MakeComboBox(bleed_base));

                    // corner radius
                    m_RectTable->setCellWidget(i, 6, c_MakeNumberEdit(corner_size_string));
                    m_RectTable->setCellWidget(i, 7, MakeComboBox(corner_base));

                    // scale
                    auto* scale_spin_box{ new QDoubleSpinBox };
                    scale_spin_box->setDecimals(1);
                    scale_spin_box->setRange(0.1, 10.0);
                    scale_spin_box->setSingleStep(0.1);
                    scale_spin_box->setValue(card_size_info.m_CardSizeScale);
                    m_RectTable->setCellWidget(i, 8, scale_spin_box);

                    // hint
                    m_RectTable->setCellWidget(i, 9, new QLineEdit{ ToQString(card_size_info.m_Hint) });
                }
                else
                {
                    const auto card_size{ card_size_info.m_SvgInfo->m_Svg.m_Size };
                    const auto [card_width, card_height]{ (card_size / UnitValue(g_Cfg.m_BaseUnit)).pod() };
                    const auto card_size_string{ QString{ "%1%3 x %2%3" }
                                                     .arg(card_width, 0, 'g', 2)
                                                     .arg(card_height, 0, 'g', 2)
                                                     .arg(ToQString(UnitShortName(g_Cfg.m_BaseUnit))) };

                    int i{ m_SvgTable->rowCount() };
                    m_SvgTable->insertRow(i);
                    m_SvgTable->setCellWidget(i, 0, new QLineEdit{ ToQString(card_name) });

                    // card size
                    auto* card_size_label{ new QLabel{ card_size_string } };
                    m_SvgTable->setCellWidget(i, 1, card_size_label);

                    // svg
                    auto* svg_combo{ MakeComboBox(
                        std::span<const std::string>{ svg_files },
                        {},
                        fs::path{ card_size_info.m_SvgInfo->m_SvgName }.stem().string()) };
                    m_SvgTable->setCellWidget(i, 2, svg_combo);
                    QObject::connect(svg_combo,
                                     &QComboBox::currentTextChanged,
                                     this,
                                     make_svg_card_size_refresh(card_size_label));

                    // input bleed
                    m_SvgTable->setCellWidget(i, 3, c_MakeNumberEdit(bleed_size_string));
                    m_SvgTable->setCellWidget(i, 4, MakeComboBox(bleed_base));

                    // hint
                    m_SvgTable->setCellWidget(i, 5, new QLineEdit{ ToQString(card_size_info.m_Hint) });
                }
            }

            // shrink the number inputs
            m_RectTable->setColumnWidth(1, 46);
            m_RectTable->setColumnWidth(2, 46);
            m_RectTable->setColumnWidth(4, 46);
            m_RectTable->setColumnWidth(6, 46);
            m_RectTable->setColumnWidth(8, 46);
        }
    };

    m_RectTable = new QTableWidget{ 0, 10 };
    m_RectTable->setHorizontalHeaderLabels({ "Card Name",
                                             "Width",
                                             "Height",
                                             "Units",
                                             "Bleed",
                                             "Units",
                                             "Corner Radius",
                                             "Units",
                                             "Scale",
                                             "Hint" });
    m_RectTable->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
    m_RectTable->setAlternatingRowColors(true);

    m_SvgTable = new QTableWidget{ 0, 6 };
    m_SvgTable->setHorizontalHeaderLabels({ "Card Name",
                                            "Size",
                                            "Svg",
                                            "Bleed",
                                            "Units",
                                            "Hint" });
    m_SvgTable->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
    m_SvgTable->setAlternatingRowColors(true);

    build_tables(config.m_CardSizes);

    auto* custom_card_shape_hint{ new QLabel{ "Drag-and-Drop an .svg file onto the main app to add custom-shaped cards." } };

    auto* new_button{ new QPushButton{ "New" } };
    auto* delete_button{ new QPushButton{ "Delete Selected" } };
    auto* restore_button{ new QPushButton{ "Restore Defaults" } };

    auto* buttons_layout{ new QHBoxLayout };
    buttons_layout->setContentsMargins(0, 0, 0, 0);
    buttons_layout->addWidget(new_button);
    buttons_layout->addWidget(delete_button);
    buttons_layout->addWidget(restore_button);

    auto* buttons{ new QWidget };
    buttons->setLayout(buttons_layout);

    auto* ok_button{ new QPushButton{ "OK" } };
    auto* apply_button{ new QPushButton{ "Apply" } };
    auto* cancel_button{ new QPushButton{ "Cancel" } };

    auto* window_buttons_layout{ new QHBoxLayout };
    window_buttons_layout->setContentsMargins(0, 0, 0, 0);
    window_buttons_layout->addWidget(ok_button);
    window_buttons_layout->addWidget(apply_button);
    window_buttons_layout->addWidget(cancel_button);

    auto* window_buttons{ new QWidget };
    window_buttons->setLayout(window_buttons_layout);

    auto* outer_layout{ new QVBoxLayout };
    outer_layout->addWidget(m_RectTable);
    outer_layout->addWidget(m_SvgTable);
    outer_layout->addWidget(custom_card_shape_hint);
    outer_layout->addWidget(buttons);
    outer_layout->addWidget(window_buttons);

    outer_layout->setAlignment(custom_card_shape_hint, Qt::AlignmentFlag::AlignCenter);

    if (m_SvgTable->rowCount() == 0)
    {
        m_SvgTable->setVisible(false);
        m_BlockResizeEventsSvgTable = true;
    }
    else
    {
        custom_card_shape_hint->setVisible(false);
    }

    setLayout(outer_layout);

    QObject::connect(m_RectTable->horizontalHeader(),
                     &QHeaderView::sectionResized,
                     this,
                     [this]()
                     {
                         AutoResizeToTable(m_RectTable);
                     });
    QObject::connect(m_SvgTable->horizontalHeader(),
                     &QHeaderView::sectionResized,
                     this,
                     [this]()
                     {
                         AutoResizeToTable(m_SvgTable);
                     });

    class FocusHistoryFilter : public QObject
    {
      public:
        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (event->type() == QEvent::FocusIn)
            {
                if (qobject_cast<QPushButton*>(qApp->focusObject()) == nullptr)
                {
                    m_LastFocusObject = qApp->focusObject();
                }
            }
            return QObject::eventFilter(obj, event);
        }

        QObject* m_LastFocusObject{ nullptr };
    };

    auto* filter{ new FocusHistoryFilter };
    filter->setParent(this);
    qApp->installEventFilter(filter);

    static constexpr auto is_ancestor_of{
        [](const QObject* ancestor, const QObject* descendant)
        {
            if (ancestor == nullptr)
            {
                return false;
            }

            while (descendant != nullptr)
            {
                if (descendant == ancestor)
                {
                    return true;
                }
                descendant = descendant->parent();
            }

            return false;
        }
    };

    QObject::connect(new_button,
                     &QPushButton::clicked,
                     [this, svg_files, filter]()
                     {
                         static constexpr auto c_MakeNumberEditFromNumber{
                             [](double number)
                             {
                                 QLocale locale{};
                                 return c_MakeNumberEdit(locale.toString(number, 'f', 1));
                             }
                         };

                         if (is_ancestor_of(m_SvgTable, filter->m_LastFocusObject))
                         {
                             const auto default_svg_name{ svg_files.front() };
                             const auto default_svg{ LoadSvg("res/card_svgs/" + default_svg_name + ".svg") };

                             const auto card_size{ default_svg.m_Size };
                             const auto [card_width, card_height]{ (card_size / UnitValue(g_Cfg.m_BaseUnit)).pod() };
                             const auto card_size_string{ QString{ "%1%3 x %2%3" }
                                                              .arg(card_width, 0, 'g', 2)
                                                              .arg(card_height, 0, 'g', 2)
                                                              .arg(ToQString(UnitShortName(g_Cfg.m_BaseUnit))) };

                             int i{ m_SvgTable->rowCount() };
                             m_SvgTable->insertRow(i);
                             m_SvgTable->setCellWidget(i, 0, new QLineEdit{ "New Card" });

                             // card size
                             auto* card_size_label{ new QLabel{ card_size_string } };
                             m_SvgTable->setCellWidget(i, 1, card_size_label);

                             // svg
                             auto* svg_combo{ MakeComboBox(
                                 std::span<const std::string>{ svg_files },
                                 {},
                                 default_svg_name) };
                             m_SvgTable->setCellWidget(i, 2, svg_combo);
                             QObject::connect(svg_combo,
                                              &QComboBox::currentTextChanged,
                                              this,
                                              make_svg_card_size_refresh(card_size_label));

                             // input bleed
                             m_SvgTable->setCellWidget(i, 3, c_MakeNumberEditFromNumber(0.1));
                             m_SvgTable->setCellWidget(i, 4, MakeComboBox(Unit::Inches));

                             // hint
                             m_SvgTable->setCellWidget(i, 5, new QLineEdit{ "Hint" });

                             m_SvgTable->selectRow(i);
                             m_SvgTable->scrollToBottom();
                         }
                         else
                         {
                             int i{ m_RectTable->rowCount() };
                             m_RectTable->insertRow(i);
                             m_RectTable->setCellWidget(i, 0, new QLineEdit{ "New Card" });

                             // card size
                             m_RectTable->setCellWidget(i, 1, c_MakeNumberEditFromNumber(3.3));
                             m_RectTable->setCellWidget(i, 2, c_MakeNumberEditFromNumber(4.4));
                             m_RectTable->setCellWidget(i, 3, MakeComboBox(Unit::Inches));

                             // input bleed
                             m_RectTable->setCellWidget(i, 4, c_MakeNumberEditFromNumber(0.1));
                             m_RectTable->setCellWidget(i, 5, MakeComboBox(Unit::Inches));

                             // corner radius
                             m_RectTable->setCellWidget(i, 6, c_MakeNumberEditFromNumber(0.1));
                             m_RectTable->setCellWidget(i, 7, MakeComboBox(Unit::Inches));

                             // scale
                             auto* scale_spin_box{ new QDoubleSpinBox };
                             scale_spin_box->setDecimals(1);
                             scale_spin_box->setRange(0.1, 10.0);
                             scale_spin_box->setSingleStep(0.1);
                             scale_spin_box->setValue(1.0);
                             m_RectTable->setCellWidget(i, 8, scale_spin_box);

                             // hint
                             m_RectTable->setCellWidget(i, 9, new QLineEdit{ "Hint" });

                             m_RectTable->selectRow(i);
                             m_RectTable->scrollToBottom();
                         }
                     });
    QObject::connect(delete_button,
                     &QPushButton::clicked,
                     [this, filter]()
                     {
                         static constexpr auto delete_rows{
                             [](const QModelIndexList& rows, QTableWidget* table)
                             {
                                 for (const auto& i : rows | std::views::reverse)
                                 {
                                     table->removeRow(i.row());
                                 }

                                 const auto first_selected_row{ rows[0].row() };
                                 if (table->rowCount() > first_selected_row)
                                 {
                                     table->selectRow(first_selected_row);
                                 }
                                 else if (first_selected_row > 0)
                                 {
                                     table->selectRow(first_selected_row - 1);
                                 }
                             }
                         };
                         const auto delete_selected_rows{
                             [this](QTableWidget* table)
                             {
                                 const auto selected_rows{ table->selectionModel()->selectedRows() };
                                 if (!selected_rows.isEmpty())
                                 {
                                     delete_rows(selected_rows, table);
                                 }
                                 else
                                 {
                                     QToolTip::showText(QCursor::pos(), "No row selected", this);
                                 }
                             }
                         };

                         if (is_ancestor_of(m_SvgTable, filter->m_LastFocusObject))
                         {
                             delete_selected_rows(m_SvgTable);
                         }
                         else
                         {
                             delete_selected_rows(m_RectTable);
                         }
                     });
    QObject::connect(restore_button,
                     &QPushButton::clicked,
                     [build_tables]()
                     {
                         build_tables(Config::g_DefaultCardSizes);
                     });

    QObject::connect(ok_button,
                     &QPushButton::clicked,
                     [this]()
                     {
                         Apply();
                         close();
                     });
    QObject::connect(apply_button,
                     &QPushButton::clicked,
                     this,
                     &CardSizePopup::Apply);
    QObject::connect(cancel_button,
                     &QPushButton::clicked,
                     this,
                     &QDialog::close);
}

CardSizePopup::~CardSizePopup()
{
}

void CardSizePopup::showEvent(QShowEvent* event)
{
    AutoResizeToTables();
    PopupBase::showEvent(event);
}

void CardSizePopup::resizeEvent(QResizeEvent* event)
{
    const auto old_width{ event->oldSize().width() };
    if (old_width > 0)
    {
        const auto new_width{ event->size().width() };
        if (new_width != old_width)
        {
            const auto window_margins{ layout()->contentsMargins() };
            const auto margins{ window_margins.left() + window_margins.right() };
            const auto fit_table{
                [this, new_width, margins](QTableWidget* table)
                {
                    const auto old_columns_width{ ComputeColumnsWidth(table) };
                    const auto table_aux_width{ ComputeAuxWidth(table) };
                    const auto new_columns_width{ new_width - margins - table_aux_width };

                    auto* header{ table->horizontalHeader() };

                    int total_columns_width{ 0 };
                    for (int i = 0; i < table->columnCount(); i++)
                    {
                        const auto relative_column_width{
                            static_cast<float>(table->columnWidth(i)) / old_columns_width
                        };
                        const auto new_column_width{
                            static_cast<int>(relative_column_width * new_columns_width)
                        };

                        header->resizeSection(i, new_column_width);
                        total_columns_width += new_column_width;
                    }

                    const auto left_over{ new_columns_width - total_columns_width };
                    const auto random_section{ rand() % table->columnCount() };
                    const auto section_width{ header->sectionSize(random_section) };
                    header->resizeSection(random_section, section_width + left_over);
                }
            };

            if (!m_BlockResizeEventsRectTable && !m_BlockResizeEventsSvgTable)
            {
                m_BlockResizeEventsRectTable = true;
                m_BlockResizeEventsSvgTable = true;
                fit_table(m_RectTable);
                fit_table(m_SvgTable);
                m_BlockResizeEventsSvgTable = false;
                m_BlockResizeEventsRectTable = false;
            }
            else if (!m_BlockResizeEventsSvgTable)
            {
                m_BlockResizeEventsSvgTable = true;
                fit_table(m_SvgTable);
                m_BlockResizeEventsSvgTable = false;
            }
            else if (!m_BlockResizeEventsRectTable)
            {
                m_BlockResizeEventsRectTable = true;
                fit_table(m_RectTable);
                m_BlockResizeEventsRectTable = false;
            }
        }
    }

    PopupBase::resizeEvent(event);
}

QByteArray CardSizePopup::GetGeometry()
{
    QByteArray geometry{ "PPP001" + PopupBase::GetGeometry() };

    const auto write_table{
        [&geometry](const QTableWidget* table)
        {
            auto* header{ table->horizontalHeader() };
            for (int i = 0; i < table->columnCount(); i++)
            {
                const auto column_width{ header->sectionSize(i) };
                static_assert(sizeof(column_width) == sizeof(int));
                const auto column_width_data{ std::bit_cast<std::array<char, sizeof(column_width)>>(column_width) };
                geometry += QByteArray::fromRawData(column_width_data.data(), column_width_data.size());
            }
        }
    };
    write_table(m_SvgTable);
    write_table(m_RectTable);

    return geometry;
}

void CardSizePopup::RestoreGeometry(const QByteArray& geometry)
{
    const auto load_table{
        [this](QTableWidget* table, const QByteArray& geometry)
        {
            const auto num_columns{ table->columnCount() };
            auto* header{ table->horizontalHeader() };
            for (int i = 0; i < num_columns; i++)
            {
                const auto column_offset{ i * sizeof(int) };
                const auto column_width_bytes{ geometry.sliced(column_offset, sizeof(int)) };
                std::array<char, sizeof(int)> column_width_data{};
                std::copy(column_width_bytes.begin(), column_width_bytes.end(), column_width_data.begin());
                const auto column_width{ std::bit_cast<int>(column_width_data) };
                header->resizeSection(i, column_width);
            }
        }
    };

    if (geometry.startsWith("PPP001"))
    {
        const auto rect_num_columns{ m_RectTable->columnCount() };
        const auto rect_columns_size{ rect_num_columns * sizeof(int) };
        const auto rect_columns_start{ geometry.size() - rect_columns_size };
        load_table(m_RectTable, geometry.sliced(rect_columns_start, rect_columns_size));

        const auto svg_num_columns{ m_SvgTable->columnCount() };
        const auto svg_columns_size{ svg_num_columns * sizeof(int) };
        const auto svg_columns_start{ rect_columns_start - svg_columns_size };
        load_table(m_SvgTable, geometry.sliced(svg_columns_start, svg_columns_size));

        const auto geometry_size{ geometry.size() - rect_columns_size - svg_columns_size - 6 };
        PopupBase::RestoreGeometry(geometry.sliced(6, geometry_size));
    }
    else
    {
        const auto num_columns{ m_RectTable->columnCount() };
        const auto columns_start{ geometry.size() - num_columns * sizeof(int) };
        load_table(m_RectTable, geometry.sliced(columns_start));

        PopupBase::RestoreGeometry(geometry.sliced(0, columns_start));
    }
}

void CardSizePopup::Apply()
{
    std::map<std::string, Config::CardSizeInfo> card_sizes{};

    QLocale locale{};
    auto get_decimals{
        [&locale](const QString& value)
        {
            const auto value_split{ value.split(locale.decimalPoint()) };
            return value_split.size() == 2 ? value_split[1].size() : 0;
        }
    };

    for (int i = 0; i < m_RectTable->rowCount(); ++i)
    {
        const auto name{
            static_cast<QLineEdit*>(m_RectTable->cellWidget(i, 0))->text().toStdString()
        };

        const auto card_width_str{
            static_cast<QLineEdit*>(m_RectTable->cellWidget(i, 1))->text()
        };
        const auto card_width{ locale.toFloat(card_width_str) };
        const auto card_height_str{
            static_cast<QLineEdit*>(m_RectTable->cellWidget(i, 2))->text()
        };
        const auto card_height{ locale.toFloat(card_height_str) };
        const auto card_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_RectTable->cellWidget(i, 3))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto card_unit_value{ UnitValue(card_unit) };
        const auto card_decimals{
            static_cast<uint32_t>(std::max(get_decimals(card_width_str), get_decimals(card_height_str))),
        };

        const auto bleed_size_str{
            static_cast<QLineEdit*>(m_RectTable->cellWidget(i, 4))->text()
        };
        const auto bleed_size{ locale.toFloat(bleed_size_str) };
        const auto bleed_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_RectTable->cellWidget(i, 5))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto bleed_unit_value{ UnitValue(bleed_unit) };
        const auto bleed_decimals{ static_cast<uint32_t>(get_decimals(bleed_size_str)) };

        const auto corner_size_str{
            static_cast<QLineEdit*>(m_RectTable->cellWidget(i, 6))->text()
        };
        const auto corner_size{ locale.toFloat(corner_size_str) };
        const auto corner_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_RectTable->cellWidget(i, 7))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto corner_unit_value{ UnitValue(corner_unit) };
        const auto corner_decimals{ static_cast<uint32_t>(get_decimals(corner_size_str)) };

        const auto scale{
            static_cast<QDoubleSpinBox*>(m_RectTable->cellWidget(i, 8))->value()
        };

        const auto hint{
            static_cast<QLineEdit*>(m_RectTable->cellWidget(i, 9))->text().toStdString()
        };

        card_sizes[std::move(name)] = {
            .m_InputBleed{
                .m_Dimension{ bleed_unit_value * bleed_size },
                .m_BaseUnit = bleed_unit,
                .m_Decimals = bleed_decimals,
            },
            .m_Hint{ std::move(hint) },
            .m_CardSizeScale = static_cast<float>(scale),

            .m_RoundedRect{ {
                .m_CardSize{
                    .m_Dimensions{
                        card_unit_value * card_width,
                        card_unit_value * card_height,
                    },
                    .m_BaseUnit = card_unit,
                    .m_Decimals = card_decimals,
                },
                .m_CornerRadius{
                    .m_Dimension{ corner_unit_value * corner_size },
                    .m_BaseUnit = corner_unit,
                    .m_Decimals = corner_decimals,
                },
            } },

            .m_SvgInfo{ std::nullopt },
        };
    }

    for (int i = 0; i < m_SvgTable->rowCount(); ++i)
    {
        const auto name{
            static_cast<QLineEdit*>(m_SvgTable->cellWidget(i, 0))->text().toStdString()
        };

        const auto svg_file_name{
            static_cast<QComboBox*>(m_SvgTable->cellWidget(i, 2))->currentText().toStdString() + ".svg"
        };
        const auto svg_file{
            "res/card_svgs/" + svg_file_name
        };

        const auto bleed_size_str{
            static_cast<QLineEdit*>(m_SvgTable->cellWidget(i, 3))->text()
        };
        const auto bleed_size{ locale.toFloat(bleed_size_str) };
        const auto bleed_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_SvgTable->cellWidget(i, 4))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto bleed_unit_value{ UnitValue(bleed_unit) };
        const auto bleed_decimals{ static_cast<uint32_t>(get_decimals(bleed_size_str)) };

        const auto hint{
            static_cast<QLineEdit*>(m_SvgTable->cellWidget(i, 5))->text().toStdString()
        };

        card_sizes[std::move(name)] = {
            .m_InputBleed{
                .m_Dimension{ bleed_unit_value * bleed_size },
                .m_BaseUnit = bleed_unit,
                .m_Decimals = bleed_decimals,
            },
            .m_Hint{ std::move(hint) },
            .m_CardSizeScale = 1.0f,

            .m_RoundedRect{ std::nullopt },

            .m_SvgInfo{
                {
                    .m_SvgName{ svg_file_name },
                    .m_Svg{ LoadSvg(svg_file) },
                },
            }
        };
    }

    CardSizesChanged(card_sizes);
}

int CardSizePopup::ComputeColumnsWidth(QTableWidget* table)
{
    int columns_width{ 0 };
    for (int i = 0; i < table->columnCount(); i++)
    {
        columns_width += table->columnWidth(i);
    }
    return columns_width;
}

int CardSizePopup::ComputeAuxWidth(QTableWidget* table)
{
    return 2 +
           table->verticalHeader()->width() +
           table->verticalScrollBar()->width();
}

int CardSizePopup::ComputeTableWidth(QTableWidget* table)
{
    return ComputeColumnsWidth(table) + ComputeAuxWidth(table);
}

int CardSizePopup::ComputeWidthToCoverTable(QTableWidget* table) const
{
    const auto table_width{ ComputeTableWidth(table) };
    const auto margins{ layout()->contentsMargins() };
    const auto minimum_width{ table_width + margins.left() + margins.right() };
    return minimum_width;
}

void CardSizePopup::AutoResizeToTable(QTableWidget* table)
{
    auto& block_flag{ table == m_RectTable
                          ? m_BlockResizeEventsRectTable
                          : m_BlockResizeEventsSvgTable };
    if (!block_flag)
    {
        const auto target_width{ ComputeWidthToCoverTable(table) };
        block_flag = true;
        resize(target_width, height());
        block_flag = false;
    }
}

void CardSizePopup::AutoResizeToTables()
{
    if (!m_BlockResizeEventsRectTable && !m_BlockResizeEventsSvgTable)
    {
        const auto target_width_rect{ ComputeWidthToCoverTable(m_RectTable) };
        const auto target_width_svg{ ComputeWidthToCoverTable(m_SvgTable) };
        if (target_width_rect > target_width_svg)
        {
            m_BlockResizeEventsRectTable = true;
            resize(target_width_rect, height());
            m_BlockResizeEventsRectTable = false;
        }
        else
        {
            m_BlockResizeEventsSvgTable = true;
            resize(target_width_svg, height());
            m_BlockResizeEventsSvgTable = false;
        }
    }
}
