#pragma once

#include <ppp/ui/popups.hpp>

class QLineEdit;

class Project;
class SelectableCardGrid;

class ImageBrowsePopup : public PopupBase
{
    Q_OBJECT

  public:
    ImageBrowsePopup(QWidget* parent,
                     const Project& project,
                     std::span<const fs::path> ignored_images = {});

    std::optional<fs::path> Show();

  private:
    QLineEdit* m_Filter{ nullptr };
    SelectableCardGrid* m_Grid{ nullptr };
};
