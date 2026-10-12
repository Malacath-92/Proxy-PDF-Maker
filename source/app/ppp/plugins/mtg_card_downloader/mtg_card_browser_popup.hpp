#pragma once

#include <ppp/ui/popups/popups.hpp>

#include <ppp/project/project_types.hpp>

class QLineEdit;

class MtGCardBrowserViewModel;
class DownloadedMtGCardGridViewModel;

class SelectableCardGrid;

class MtGCardBrowserPopup : public PopupBase
{
    Q_OBJECT

  public:
    MtGCardBrowserPopup(QWidget* parent,
                        MtGCardBrowserViewModel* view_model);

    std::optional<QString> Show();
    std::optional<ImagePreview> GetSelectedPreview() const;

    std::optional<QString> GetSelectedBackside() const;
    std::optional<QString> GetSelectedBacksideURI() const;
    std::optional<ImagePreview> GetSelectedBacksidePreview() const;

    enum class Choice
    {
        Ok,
        Cancel,
    };
    Choice GetChoice() const;

  private:
    void CloseWithChoice(Choice choice);

    MtGCardBrowserViewModel& m_ViewModel;

    QLineEdit* m_Filter{ nullptr };

    QLineEdit* m_ScryfallQuery{ nullptr };
    DownloadedMtGCardGridViewModel* m_GridViewModel{ nullptr };
    SelectableCardGrid* m_Grid{ nullptr };

    Choice m_Choice{ Choice::Ok };
};
