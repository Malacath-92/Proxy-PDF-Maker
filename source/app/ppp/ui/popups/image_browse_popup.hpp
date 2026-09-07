#pragma once

#include <ppp/ui/popups/popups.hpp>

#include <ppp/project/project_types.hpp>

class QLineEdit;

class ImageBrowseViewModel;
class SelectableCardGrid;

class ImageBrowsePopup : public PopupBase
{
    Q_OBJECT

  public:
    ImageBrowsePopup(QWidget* parent,
                     ImageBrowseViewModel* view_model);

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

    const ImageBrowseViewModel& m_ViewModel;

    QLineEdit* m_Filter{ nullptr };
    SelectableCardGrid* m_Grid{ nullptr };

    Choice m_Choice{ Choice::Ok };
};
