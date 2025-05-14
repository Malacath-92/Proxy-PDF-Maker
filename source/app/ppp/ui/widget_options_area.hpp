#pragma once

#include <QScrollArea>

class OptionsAreaWidget : public QScrollArea
{
  public:
    OptionsAreaWidget(QWidget* actions,
                      QWidget* print_options,
                      QWidget* guides_options,
                      QWidget* card_options,
                      QWidget* global_options);
};
