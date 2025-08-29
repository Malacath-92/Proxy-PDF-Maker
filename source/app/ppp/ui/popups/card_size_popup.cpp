#include <ppp/ui/popups/card_size_popup.hpp>

#include <magic_enum/magic_enum.hpp>

#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QModelIndex>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>

#include <ppp/ui/widget_combo_box.hpp>

CardSizePopup::CardSizePopup(QWidget* parent,
                             const Config& config)
    : PopupBase{ parent }
{
    m_AutoCenter = true;
    setWindowFlags(Qt::WindowType::Dialog);

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

            for (const auto& [card_name, card_size_info] : card_sizes)
            {
                const auto& [card_size, card_base, card_decimals]{ card_size_info.m_CardSize };
                const auto card_unit_value{ UnitValue(card_base) };
                const auto [card_width, card_height]{ (card_size / card_unit_value).pod() };
                const auto card_width_string{ fmt::format("{:.{}f}", card_width, card_decimals) };
                const auto card_height_string{ fmt::format("{:.{}f}", card_height, card_decimals) };

                const auto& [bleed_size, bleed_base, bleed_decimals]{ card_size_info.m_InputBleed };
                const auto bleed_unit_value{ UnitValue(bleed_base) };
                const auto bleed_size_string{ fmt::format("{:.{}f}", bleed_size / bleed_unit_value, bleed_decimals) };

                const auto& [corner_size, corner_base, corner_decimals]{ card_size_info.m_CornerRadius };
                const auto corner_unit_value{ UnitValue(corner_base) };
                const auto corner_size_string{ fmt::format("{:.{}f}", corner_size / corner_unit_value, corner_decimals) };

                int i{ m_Table->rowCount() };
                m_Table->insertRow(i);
                m_Table->setCellWidget(i, 0, new QLineEdit{ ToQString(card_name) });

                // card size
                m_Table->setCellWidget(i, 1, c_MakeNumberEdit(ToQString(card_width_string)));
                m_Table->setCellWidget(i, 2, c_MakeNumberEdit(ToQString(card_height_string)));
                m_Table->setCellWidget(i, 3, MakeComboBox(card_base));

                // input bleed
                m_Table->setCellWidget(i, 4, c_MakeNumberEdit(ToQString(bleed_size_string)));
                m_Table->setCellWidget(i, 5, MakeComboBox(bleed_base));

                // corner radius
                m_Table->setCellWidget(i, 6, c_MakeNumberEdit(ToQString(corner_size_string)));
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

    auto* table_wrapper_layout{ new QHBoxLayout };
    table_wrapper_layout->setContentsMargins(0, 0, 0, 0);
    table_wrapper_layout->addStretch();
    table_wrapper_layout->addWidget(m_Table);
    table_wrapper_layout->addStretch();

    auto* table_wrapper{ new QWidget };
    table_wrapper->setLayout(table_wrapper_layout);

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
    outer_layout->addWidget(table_wrapper);
    outer_layout->addWidget(buttons);
    outer_layout->addWidget(window_buttons);

    setLayout(outer_layout);

    QObject::connect(new_button,
                     &QPushButton::clicked,
                     [this]()
                     {
                         int i{ m_Table->rowCount() };
                         m_Table->insertRow(i);
                         m_Table->setCellWidget(i, 0, new QLineEdit{ "New Card" });

                         // card size
                         m_Table->setCellWidget(i, 1, c_MakeNumberEdit("3.3"));
                         m_Table->setCellWidget(i, 2, c_MakeNumberEdit("4.4"));
                         m_Table->setCellWidget(i, 3, MakeComboBox(Unit::Inches));

                         // input bleed
                         m_Table->setCellWidget(i, 4, c_MakeNumberEdit("0.1"));
                         m_Table->setCellWidget(i, 5, MakeComboBox(Unit::Inches));

                         // corner radius
                         m_Table->setCellWidget(i, 4, c_MakeNumberEdit("0.1"));
                         m_Table->setCellWidget(i, 5, MakeComboBox(Unit::Inches));

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
                         for (const auto& i : m_Table->selectionModel()->selectedRows())
                         {
                             m_Table->removeRow(i.row());
                         }
                     });
    QObject::connect(restore_button,
                     &QPushButton::clicked,
                     [build_table]()
                     {
                         build_table(Config::m_DefaultCardSizes);
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
    {
        // Hack the right size for the table...
        int table_width{ 2 +
                         m_Table->verticalHeader()->width() +
                         m_Table->verticalScrollBar()->width() };
        for (int i = 0; i < m_Table->columnCount(); i++)
        {
            table_width += m_Table->columnWidth(i);
        }
        m_Table->setFixedWidth(table_width);
    }

    const auto margins{ layout()->contentsMargins() };
    const auto minimum_width{ m_Table->width() + margins.left() + margins.right() };
    setMinimumWidth(minimum_width);

    PopupBase::showEvent(event);
}

void CardSizePopup::Apply()
{
    std::map<std::string, Config::CardSizeInfo> card_sizes{};

    static constexpr auto c_GetDecimals{
        [](const QString& value)
        {
            const auto value_split{ value.split(".") };
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
        const auto card_width{ card_width_str.toFloat() };
        const auto card_height_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 2))->text()
        };
        const auto card_height{ card_height_str.toFloat() };
        const auto card_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 3))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto card_unit_value{ UnitValue(card_unit) };
        const auto card_decimals{
            static_cast<uint32_t>(std::max(c_GetDecimals(card_width_str), c_GetDecimals(card_height_str))),
        };

        const auto bleed_size_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 4))->text()
        };
        const auto bleed_size{ bleed_size_str.toFloat() };
        const auto bleed_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 5))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto bleed_unit_value{ UnitValue(bleed_unit) };
        const auto bleed_decimals{ static_cast<uint32_t>(c_GetDecimals(bleed_size_str)) };

        const auto corner_size_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 6))->text()
        };
        const auto corner_size{ corner_size_str.toFloat() };
        const auto corner_unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 7))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto corner_unit_value{ UnitValue(corner_unit) };
        const auto corner_decimals{ static_cast<uint32_t>(c_GetDecimals(corner_size_str)) };

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
