#include <ppp/ui/popups/card_size_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <QCursor>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QModelIndex>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>

#include <ppp/ui/widget_combo_box.hpp>

CardSizePopup::CardSizePopup(QWidget* parent,
                             const Config& config)
    : PopupBase{ parent }
{
    m_AutoCenter = false;
    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("Edit Card Sizes");

    static constexpr auto c_MakeNumberEdit{
        [](QString initial_string)
        {
            auto* line_edit{ new QLineEdit{ std::move(initial_string) } };
            line_edit->setValidator(new QDoubleValidator{ -999, 999, 4 });
            return line_edit;
        }
    };

    auto build_table{
        [this](const auto& card_sizes)
        {
            m_Table->clearContents();
            m_Table->setRowCount(0);

            QLocale locale{};
            for (const auto& [card_name, card_size_info] : card_sizes)
            {
                const auto& [card_size, card_base, card_decimals]{ card_size_info.m_CardSize };
                const auto card_unit_value{ UnitValue(card_base) };
                const auto [card_width, card_height]{ (card_size / card_unit_value).pod() };
                const auto card_width_string{ locale.toString(card_width, 'f', card_decimals) };
                const auto card_height_string{ locale.toString(card_height, 'f', card_decimals) };

                const auto& [bleed_size, bleed_base, bleed_decimals]{ card_size_info.m_InputBleed };
                const auto bleed_unit_value{ UnitValue(bleed_base) };
                const auto bleed_size_string{ locale.toString(bleed_size / bleed_unit_value, 'f', bleed_decimals) };

                const auto& [corner_size, corner_base, corner_decimals]{ card_size_info.m_CornerRadius };
                const auto corner_unit_value{ UnitValue(corner_base) };
                const auto corner_size_string{ locale.toString(corner_size / corner_unit_value, 'f', corner_decimals) };

                int i{ m_Table->rowCount() };
                m_Table->insertRow(i);
                m_Table->setCellWidget(i, 0, new QLineEdit{ ToQString(card_name) });

                // card size
                m_Table->setCellWidget(i, 1, c_MakeNumberEdit(card_width_string));
                m_Table->setCellWidget(i, 2, c_MakeNumberEdit(card_height_string));
                m_Table->setCellWidget(i, 3, MakeComboBox(card_base));

                // input bleed
                m_Table->setCellWidget(i, 4, c_MakeNumberEdit(bleed_size_string));
                m_Table->setCellWidget(i, 5, MakeComboBox(bleed_base));

                // corner radius
                m_Table->setCellWidget(i, 6, c_MakeNumberEdit(corner_size_string));
                m_Table->setCellWidget(i, 7, MakeComboBox(corner_base));

                // scale
                auto* scale_spin_box{ new QDoubleSpinBox };
                scale_spin_box->setDecimals(1);
                scale_spin_box->setRange(0.1, 10.0);
                scale_spin_box->setSingleStep(0.1);
                scale_spin_box->setValue(card_size_info.m_CardSizeScale);
                m_Table->setCellWidget(i, 8, scale_spin_box);

                // hint
                m_Table->setCellWidget(i, 9, new QLineEdit{ ToQString(card_size_info.m_Hint) });
            }

            // shrink the number inputs
            m_Table->setColumnWidth(1, 46);
            m_Table->setColumnWidth(2, 46);
            m_Table->setColumnWidth(4, 46);
            m_Table->setColumnWidth(6, 46);
            m_Table->setColumnWidth(8, 46);
        }
    };

    m_Table = new QTableWidget{ 0, 10 };
    m_Table->setHorizontalHeaderLabels({ "Paper Name",
                                         "Width",
                                         "Height",
                                         "Units",
                                         "Bleed",
                                         "Units",
                                         "Corner Radius",
                                         "Units",
                                         "Scale",
                                         "Hint" });
    m_Table->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
    m_Table->setAlternatingRowColors(true);
    build_table(config.m_CardSizes);

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
    outer_layout->addWidget(m_Table);
    outer_layout->addWidget(buttons);
    outer_layout->addWidget(window_buttons);

    setLayout(outer_layout);

    QObject::connect(m_Table->horizontalHeader(),
                     &QHeaderView::sectionResized,
                     this,
                     [this]()
                     {
                         AutoResizeToTable();
                     });

    QObject::connect(new_button,
                     &QPushButton::clicked,
                     [this]()
                     {
                         static constexpr auto c_MakeNumberEditFromNumber{
                             [](double number)
                             {
                                 QLocale locale{};
                                 return c_MakeNumberEdit(locale.toString(number, 'f', 1));
                             }
                         };

                         int i{ m_Table->rowCount() };
                         m_Table->insertRow(i);
                         m_Table->setCellWidget(i, 0, new QLineEdit{ "New Card" });

                         // card size
                         m_Table->setCellWidget(i, 1, c_MakeNumberEditFromNumber(3.3));
                         m_Table->setCellWidget(i, 2, c_MakeNumberEditFromNumber(4.4));
                         m_Table->setCellWidget(i, 3, MakeComboBox(Unit::Inches));

                         // input bleed
                         m_Table->setCellWidget(i, 4, c_MakeNumberEditFromNumber(0.1));
                         m_Table->setCellWidget(i, 5, MakeComboBox(Unit::Inches));

                         // corner radius
                         m_Table->setCellWidget(i, 6, c_MakeNumberEditFromNumber(0.1));
                         m_Table->setCellWidget(i, 7, MakeComboBox(Unit::Inches));

                         // scale
                         auto* scale_spin_box{ new QDoubleSpinBox };
                         scale_spin_box->setDecimals(1);
                         scale_spin_box->setRange(0.1, 10.0);
                         scale_spin_box->setSingleStep(0.1);
                         scale_spin_box->setValue(1.0);
                         m_Table->setCellWidget(i, 8, scale_spin_box);

                         // hint
                         m_Table->setCellWidget(i, 9, new QLineEdit{ "Hint" });

                         m_Table->selectRow(i);
                         m_Table->scrollToBottom();
                     });
    QObject::connect(delete_button,
                     &QPushButton::clicked,
                     [this]()
                     {
                         const auto selected_rows{ m_Table->selectionModel()->selectedRows() };
                         if (selected_rows.isEmpty())
                         {
                             QToolTip::showText(QCursor::pos(), "No row selected", this);
                             return;
                         }

                         for (const auto& i : selected_rows | std::views::reverse)
                         {
                             m_Table->removeRow(i.row());
                         }

                         const auto first_selected_row{ selected_rows[0].row() };
                         if (m_Table->rowCount() > first_selected_row)
                         {
                             m_Table->selectRow(first_selected_row);
                         }
                         else if (first_selected_row > 0)
                         {
                             m_Table->selectRow(first_selected_row - 1);
                         }
                     });
    QObject::connect(restore_button,
                     &QPushButton::clicked,
                     [build_table]()
                     {
                         build_table(Config::g_DefaultCardSizes);
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
    AutoResizeToTable();
    PopupBase::showEvent(event);
}

void CardSizePopup::resizeEvent(QResizeEvent* event)
{
    if (!m_BlockResizeEvents)
    {
        const auto old_width{ event->oldSize().width() };
        if (old_width > 0)
        {
            const auto new_width{ event->size().width() };
            const auto del_width{ new_width - old_width };

            if (del_width != 0)
            {
                m_BlockResizeEvents = true;

                const auto old_columns_width{ ComputeColumnsWidth() };
                const auto new_columns_width{ old_columns_width + del_width };

                auto* header{ m_Table->horizontalHeader() };

                int total_columns_width{ 0 };
                for (int i = 0; i < m_Table->columnCount(); i++)
                {
                    const auto relative_column_width{
                        static_cast<float>(m_Table->columnWidth(i)) / old_columns_width
                    };
                    const auto new_column_width{
                        static_cast<int>(relative_column_width * new_columns_width)
                    };

                    header->resizeSection(i, new_column_width);
                    total_columns_width += new_column_width;
                }

                const auto left_over{ new_columns_width - total_columns_width };
                const auto random_section{ rand() % m_Table->columnCount() };
                const auto section_width{ header->sectionSize(random_section) };
                header->resizeSection(random_section, section_width + left_over);

                m_BlockResizeEvents = false;
            }
        }
    }

    PopupBase::resizeEvent(event);
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

    for (int i = 0; i < m_Table->rowCount(); ++i)
    {
        const auto name{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 0))->text().toStdString()
        };

        const auto card_width_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 1))->text()
        };
        const auto card_width{ locale.toFloat(card_width_str) };
        const auto card_height_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 2))->text()
        };
        const auto card_height{ locale.toFloat(card_height_str) };
        const auto card_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 3))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto card_unit_value{ UnitValue(card_unit) };
        const auto card_decimals{
            static_cast<uint32_t>(std::max(get_decimals(card_width_str), get_decimals(card_height_str))),
        };

        const auto bleed_size_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 4))->text()
        };
        const auto bleed_size{ locale.toFloat(bleed_size_str) };
        const auto bleed_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 5))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto bleed_unit_value{ UnitValue(bleed_unit) };
        const auto bleed_decimals{ static_cast<uint32_t>(get_decimals(bleed_size_str)) };

        const auto corner_size_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 6))->text()
        };
        const auto corner_size{ locale.toFloat(corner_size_str) };
        const auto corner_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 7))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto corner_unit_value{ UnitValue(corner_unit) };
        const auto corner_decimals{ static_cast<uint32_t>(get_decimals(corner_size_str)) };

        const auto scale{
            static_cast<QDoubleSpinBox*>(m_Table->cellWidget(i, 8))->value()
        };

        const auto hint{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 9))->text().toStdString()
        };

        card_sizes[std::move(name)] = {
            .m_CardSize{
                .m_Dimensions{
                    card_unit_value * card_width,
                    card_unit_value * card_height,
                },
                .m_BaseUnit = card_unit,
                .m_Decimals = card_decimals,
            },
            .m_InputBleed{
                .m_Dimension{ bleed_unit_value * bleed_size },
                .m_BaseUnit = bleed_unit,
                .m_Decimals = bleed_decimals,
            },
            .m_CornerRadius{
                .m_Dimension{ corner_unit_value * corner_size },
                .m_BaseUnit = corner_unit,
                .m_Decimals = corner_decimals,
            },
            .m_Hint{ std::move(hint) },
            .m_CardSizeScale = static_cast<float>(scale),
        };
    }

    CardSizesChanged(card_sizes);
}

int CardSizePopup::ComputeColumnsWidth()
{
    int columns_width{ 0 };
    for (int i = 0; i < m_Table->columnCount(); i++)
    {
        columns_width += m_Table->columnWidth(i);
    }
    return columns_width;
}

int CardSizePopup::ComputeTableWidth()
{
    const auto table_width{ 2 +
                            m_Table->verticalHeader()->width() +
                            m_Table->verticalScrollBar()->width() +
                            ComputeColumnsWidth() };
    return table_width;
}

int CardSizePopup::ComputeWidthToCoverTable()
{
    const auto table_width{ ComputeTableWidth() };
    const auto margins{ layout()->contentsMargins() };
    const auto minimum_width{ table_width + margins.left() + margins.right() };
    return minimum_width;
}

void CardSizePopup::AutoResizeToTable()
{
    if (!m_BlockResizeEvents)
    {
        m_BlockResizeEvents = true;
        resize(ComputeWidthToCoverTable(), height());
        m_BlockResizeEvents = false;
    }
}
