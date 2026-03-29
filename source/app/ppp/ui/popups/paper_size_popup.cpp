#include <ppp/ui/popups/paper_size_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <QCursor>
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

PaperSizePopup::PaperSizePopup(QWidget* parent,
                               const Config& config)
    : PopupBase{ parent }
{
    m_AutoCenter = false;
    m_PersistGeometry = true;

    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("Edit Paper Sizes");
    setObjectName("PaperSizePopup");

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

            QLocale locale{};
            for (const auto& [paper_name, paper_size_info] : paper_sizes)
            {
                if (paper_name == Config::c_FitSize || paper_name == Config::c_BasePDFSize)
                {
                    continue;
                }

                const auto& [size, base, decimals]{ paper_size_info };
                const auto unit_value{ UnitValue(base) };
                const auto [width, height]{ (size / unit_value).pod() };
                const auto width_string{ locale.toString(width, 'f', decimals) };
                const auto height_string{ locale.toString(height, 'f', decimals) };

                int i{ m_Table->rowCount() };
                m_Table->insertRow(i);
                m_Table->setCellWidget(i, 0, new QLineEdit{ ToQString(paper_name) });
                m_Table->setCellWidget(i, 1, c_MakeNumberEdit(width_string));
                m_Table->setCellWidget(i, 2, c_MakeNumberEdit(height_string));
                m_Table->setCellWidget(i, 3, MakeComboBox(base));
            }
        }
    };

    m_Table = new QTableWidget{ 0, 4 };
    m_Table->setHorizontalHeaderLabels({ "Paper Name", "Width", "Height", "Units" });
    m_Table->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
    m_Table->setAlternatingRowColors(true);
    build_table(config.m_PageSizes);

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
                         m_Table->setCellWidget(i, 0, new QLineEdit{ "New Paper" });
                         m_Table->setCellWidget(i, 1, c_MakeNumberEditFromNumber(40.4));
                         m_Table->setCellWidget(i, 2, c_MakeNumberEditFromNumber(40.4));
                         m_Table->setCellWidget(i, 3, MakeComboBox(Unit::Inches));

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
                         build_table(Config::g_DefaultPageSizes);
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
    AutoResizeToTable();
    PopupBase::showEvent(event);
}

void PaperSizePopup::resizeEvent(QResizeEvent* event)
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

QByteArray PaperSizePopup::GetGeometry()
{
    QByteArray geometry{ PopupBase::GetGeometry() };

    auto* header{ m_Table->horizontalHeader() };
    for (int i = 0; i < m_Table->columnCount(); i++)
    {
        const auto column_width{ header->sectionSize(i) };
        static_assert(sizeof(column_width) == sizeof(int));
        const auto column_width_data{ std::bit_cast<std::array<char, sizeof(column_width)>>(column_width) };
        geometry += QByteArray::fromRawData(column_width_data.data(), column_width_data.size());
    }

    return geometry;
}

void PaperSizePopup::RestoreGeometry(const QByteArray& geometry)
{
    const auto num_columns{ m_Table->columnCount() };
    const auto columns_start{ geometry.size() - num_columns * sizeof(int) };

    auto* header{ m_Table->horizontalHeader() };
    for (int i = 0; i < num_columns; i++)
    {
        const auto column_offset{ i * sizeof(int) };
        const auto column_width_bytes{ geometry.sliced(columns_start + column_offset, sizeof(int)) };
        std::array<char, sizeof(int)> column_width_data{};
        std::copy(column_width_bytes.begin(), column_width_bytes.end(), column_width_data.begin());
        const auto column_width{ std::bit_cast<int>(column_width_data) };
        header->resizeSection(i, column_width);
    }

    PopupBase::RestoreGeometry(geometry.sliced(0, columns_start));
}

void PaperSizePopup::Apply()
{
    std::map<std::string, Config::SizeInfo> page_sizes{
        { std::string{ Config::c_FitSize }, {} },
        { std::string{ Config::c_BasePDFSize }, {} },
    };

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
        const auto width_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 1))->text()
        };
        const auto width{ locale.toFloat(width_str) };
        const auto height_str{
            static_cast<QLineEdit*>(m_Table->cellWidget(i, 2))->text()
        };
        const auto height{ locale.toFloat(height_str) };
        const auto unit{
            magic_enum::enum_cast<Unit>(
                static_cast<QComboBox*>(m_Table->cellWidget(i, 3))->currentText().toStdString())
                .value_or(Unit::Millimeter)
        };
        const auto unit_value{ UnitValue(unit) };
        const auto decimals{
            static_cast<uint32_t>(std::max(get_decimals(width_str), get_decimals(height_str))),
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

int PaperSizePopup::ComputeColumnsWidth()
{
    int columns_width{ 0 };
    for (int i = 0; i < m_Table->columnCount(); i++)
    {
        columns_width += m_Table->columnWidth(i);
    }
    return columns_width;
}

int PaperSizePopup::ComputeTableWidth()
{
    const auto table_width{ 2 +
                            m_Table->verticalHeader()->width() +
                            m_Table->verticalScrollBar()->width() +
                            ComputeColumnsWidth() };
    return table_width;
}

int PaperSizePopup::ComputeWidthToCoverTable()
{
    const auto table_width{ ComputeTableWidth() };
    const auto margins{ layout()->contentsMargins() };
    const auto minimum_width{ table_width + margins.left() + margins.right() };
    return minimum_width;
}

void PaperSizePopup::AutoResizeToTable()
{
    if (!m_BlockResizeEvents)
    {
        m_BlockResizeEvents = true;
        resize(ComputeWidthToCoverTable(), height());
        m_BlockResizeEvents = false;
    }
}
