#pragma once

#include <span>
#include <string_view>

#include <QComboBox>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

template<class StringT>
static void UpdateComboBox(QComboBox* combo_box,
                           std::span<StringT> options,
                           std::span<StringT> tooltips,
                           std::string_view default_option)
{
    combo_box->blockSignals(true);
    combo_box->clear();

    for (const auto& option : options)
    {
        combo_box->addItem(ToQString(option));
    }

    for (size_t i = 0; i < tooltips.size(); i++)
    {
        if (!tooltips[i].empty())
        {
            combo_box->setItemData(
                static_cast<int>(i),
                ToQString(tooltips[i]),
                Qt::ToolTipRole);
        }
    }

    combo_box->blockSignals(false);

    if (std::ranges::contains(options, default_option))
    {
        combo_box->setCurrentText(ToQString(default_option));
    }
}

template<class StringT>
QComboBox* MakeComboBox(std::span<StringT> options,
                        std::span<StringT> tooltips,
                        std::string_view default_option)
{
    auto* combo_box{ new QComboBox };
    UpdateComboBox(combo_box, options, tooltips, default_option);
    return combo_box;
}

template<Enum EnumT>
static void UpdateComboBox(QComboBox* combo_box, EnumT default_option)
{
    return UpdateComboBox(std::span<const std::string_view>{ magic_enum::enum_names<EnumT>() },
                          {},
                          magic_enum::enum_name(default_option));
}

template<Enum EnumT>
static QComboBox* MakeComboBox(EnumT default_option)
{
    return MakeComboBox(std::span<const std::string_view>{ magic_enum::enum_names<EnumT>() },
                        {},
                        magic_enum::enum_name(default_option));
}
