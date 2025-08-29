#include <ppp/ui/popups/paper_size_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

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

PaperSizePopup::PaperSizePopup(QWidget* parent,
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
        [this](const auto& paper_sizes)
        {
            m_Table->clearContents();
            m_Table->setRowCount(0);

            for (const auto& [paper_name, paper_size_info] : paper_sizes)
            {
                if (paper_name == Config::c_FitSize || paper_name == Config::c_BasePDFSize)
                {
                    continue;
                }

                const auto& [size, base, decimals]{ paper_size_info };
                const auto unit_value{ UnitValue(base) };
                const auto [width, height]{ (size / unit_value).pod() };
                const auto width_string{ fmt::format("{:.{}f}", width, decimals) };
                const auto height_string{ fmt::format("{:.{}f}", height, decimals) };

                int i{ m_Table->rowCount() };
                m_Table->insertRow(i);
                m_Table->setCellWidget(i, 0, new QLineEdit{ ToQString(paper_name) });
                m_Table->setCellWidget(i, 1, c_MakeNumberEdit(ToQString(width_string)));
                m_Table->setCellWidget(i, 2, c_MakeNumberEdit(ToQString(height_string)));
                m_Table->setCellWidget(i, 3, MakeComboBox(base));
            }
        }
    };

    m_Table = new QTableWidget{ 0, 4 };
    m_Table->setHorizontalHeaderLabels({ "Paper Name", "Width", "Height", "Units" });
    m_Table->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
    m_Table->setAlternatingRowColors(true);
    build_table(config.m_PageSizes);

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
                         m_Table->setCellWidget(i, 0, new QLineEdit{ "New Paper" });
                         m_Table->setCellWidget(i, 1, c_MakeNumberEdit("40.4"));
                         m_Table->setCellWidget(i, 2, c_MakeNumberEdit("40.4"));
                         m_Table->setCellWidget(i, 3, MakeComboBox(Unit::Inches));

                         m_Table->selectRow(i);
                         m_Table->scrollToBottom();
                     });
    QObject::connect(delete_button,
                     &QPushButton::clicked,
                     [this]()
                     {
                         const auto selected_rows{ m_Table->selectionModel()->selectedRows() };
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
                         build_table(Config::m_DefaultPageSizes);
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
                     &PaperSizePopup::Apply);
    QObject::connect(cancel_button,
                     &QPushButton::clicked,
                     this,
                     &QDialog::close);
}

PaperSizePopup::~PaperSizePopup()
{
}

void PaperSizePopup::showEvent(QShowEvent* event)
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

void PaperSizePopup::Apply()
{
    std::map<std::string, Config::SizeInfo> page_sizes{
        { std::string{ Config::c_FitSize }, {} },
        { std::string{ Config::c_BasePDFSize }, {} },
    };

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
        const auto width_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 1))->text()
        };
        const auto width{ width_str.toFloat() };
        const auto height_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 2))->text()
        };
        const auto height{ height_str.toFloat() };
        const auto unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 3))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto unit_value{ UnitValue(unit) };
        const auto decimals{
            static_cast<uint32_t>(std::max(c_GetDecimals(width_str), c_GetDecimals(height_str))),
        };

        page_sizes[std::move(name)] = {
            .m_Dimensions{
                unit_value * width,
                unit_value * height,
            },
            .m_BaseUnit = unit,
            .m_Decimals = decimals,
        };
    }

    PageSizesChanged(page_sizes);
}
