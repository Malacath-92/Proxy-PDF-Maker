#pragma once

#include <QFrame>

#include <ppp/util.hpp>

class CardViewModel;
class CardImage;

class SelectableCard : public QFrame
{
    Q_OBJECT

  public:
    SelectableCard(CardViewModel* view_model);

    bool ToggleSelected();
    void Select();
    void Unselect();

    const fs::path& GetCardName() const;

    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

  private:
    CardViewModel* m_ViewModel;
    CardImage* m_CardImage{ nullptr };
    bool m_Selected{ false };
};
