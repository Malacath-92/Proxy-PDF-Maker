#pragma once

#include <QScrollArea>

class PrintProxyPrepApplication;

class OptionsAreaWidget : public QScrollArea
{
    Q_OBJECT

  public:
    OptionsAreaWidget(const PrintProxyPrepApplication& app,
                      QWidget* actions,
                      QWidget* print_options,
                      QWidget* guides_options,
                      QWidget* card_options,
                      QWidget* global_options);

  signals:
    void SetObjectVisibility(QString object_name, bool visible);
};
