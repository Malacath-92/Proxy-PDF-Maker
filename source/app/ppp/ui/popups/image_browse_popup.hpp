#pragma once

#include <ppp/ui/popups.hpp>

class Project;
class SelectableCardGrid;

class ImageBrowsePopup : public PopupBase
{
    Q_OBJECT

  public:
    ImageBrowsePopup(QWidget* parent, const Project& project);

    std::optional<fs::path> Show();

  private:
    SelectableCardGrid* m_Grid{ nullptr };
};
