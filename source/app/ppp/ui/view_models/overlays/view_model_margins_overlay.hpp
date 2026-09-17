#pragma once

#include <QObject>

#include <ppp/project/project_types.hpp>

class Project;

class MarginsOverlayViewModel : public QObject
{
  public:
    MarginsOverlayViewModel(const Project& project,
                            bool is_backside);

    Size GetPageSize() const;
    Margins GetPageMargins() const;

  private:
    const Project& m_Project;
    const bool m_IsBackside;
};
