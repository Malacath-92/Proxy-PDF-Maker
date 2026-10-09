#pragma once

#include <span>

#include <QObject>

#include <ppp/project/project_types.hpp>

class SelectableCardGridViewModel;

class Project;

class ImageBrowseViewModel : public QObject
{
    Q_OBJECT

  public:
    ImageBrowseViewModel(const Project& project,
                         std::span<const fs::path> ignored_images = {});

    bool HasCards() const;
    bool HasIgnoredCards() const;

    SelectableCardGridViewModel* MakeGridViewModel();

  private:
    const Project& m_Project;
    std::span<const fs::path> m_IgnoredImages;
};
