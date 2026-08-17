#pragma once

#include <span>
#include <string_view>

#include <magic_enum/magic_enum.hpp>

#include <QComboBox>

#include <ppp/concepts.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

QComboBox* MakeComboBox();

inline void UpdateComboBox(QComboBox* combo_box,
                           RangeOfStringLike auto options,
                           StringLike auto default_option)
{
    combo_box->blockSignals(true);
    combo_box->clear();

    for (const auto& option : options)
    {
        combo_box->addItem(ToQString(option));
    }

    combo_box->blockSignals(false);

    if (std::ranges::contains(options, default_option))
    {
        combo_box->setCurrentText(ToQString(default_option));
    }
}

inline void UpdateComboBox(QComboBox* combo_box,
                           RangeOfStringLike auto options,
                           RangeOfStringLike auto tooltips,
                           StringLike auto default_option)
{
    UpdateComboBox(combo_box,
                   options,
                   default_option);

    int i{ 0 };
    for (const auto& tooltip : tooltips)
    {
        if (!tooltip.empty())
        {
            combo_box->setItemData(
                i,
                ToQString(tooltip),
                Qt::ToolTipRole);
        }
        ++i;
    }
}

QComboBox* MakeComboBox(RangeOfStringLike auto options,
                        RangeOfStringLike auto tooltips,
                        StringLike auto default_option)
{
    auto* combo_box{ MakeComboBox() };
    UpdateComboBox(combo_box,
                   options,
                   tooltips,
                   default_option);
    return combo_box;
}

QComboBox* MakeComboBox(RangeOfStringLike auto options,
                        StringLike auto default_option)
{
    auto* combo_box{ MakeComboBox() };
    UpdateComboBox(combo_box,
                   options,
                   default_option);
    return combo_box;
}

template<Enum EnumT>
static QComboBox* MakeComboBox(EnumT default_option)
{
    return MakeComboBox(magic_enum::enum_names<EnumT>(),
                        magic_enum::enum_name(default_option));
}
