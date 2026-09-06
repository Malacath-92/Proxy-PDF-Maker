#pragma once

#include <QObject>

#include <ppp/util.hpp>

#include <ppp/ui/view_models/card_view_params.hpp>

class Project;

class CardViewModel : public QObject
{
    Q_OBJECT

    // friend class CardOptionsWidget;

  public:
    CardViewModel(fs::path card_name,
                  CardViewParams params,
                  const Project& project);

  signals:
    // forward

  public slots:

  private slots:

  private:
    void EmitDefaults();

    fs::path m_CardName;
    const CardViewParams m_ViewParams;

    const Project& m_Project;
};
