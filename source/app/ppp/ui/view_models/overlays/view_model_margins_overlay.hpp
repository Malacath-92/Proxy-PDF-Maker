#pragma once

#include <QObject>

#include <ppp/project/project_types.hpp>

class Project;

class MarginsOverlayViewModel : public QObject
{
    Q_OBJECT

  public:
    MarginsOverlayViewModel(const Project& project,
                            bool is_backside);

    bool ShouldDrawMargins() const;

    Size GetPageSize() const;
    Margins GetPageMargins() const;

  signals:
    void Redraw();

  private:
    const Project& m_Project;
    const bool m_IsBackside;
};
