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

    enum class Choice
    {
        Ok,
        Clear,
        Reset,
        Cancel,
    };
    Choice GetChoice() const;

  private:
    void CloseWithChoice(Choice choice);

    QLineEdit* m_Filter{ nullptr };
    SelectableCardGrid* m_Grid{ nullptr };

    Choice m_Choice{ Choice::Ok };
};
