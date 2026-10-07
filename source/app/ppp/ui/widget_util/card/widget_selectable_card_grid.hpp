#pragma once

#include <optional>
#include <vector>

#include <QWidget>

#include <ppp/util.hpp>

class SelectableCardGridViewModel;

class SelectableCard;

class SelectableCardGrid : public QWidget
{
    Q_OBJECT

  public:
    SelectableCardGrid(SelectableCardGridViewModel* view_model);

    void ApplyFilter(const QString& filter);

    int TotalWidthFromItemWidth(int item_width) const;

    std::optional<fs::path> GetSelectedCardName() const;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

    virtual void resizeEvent(QResizeEvent* event) override;

    virtual bool eventFilter(QObject* obj, QEvent* event) override;

  public:
    SelectableCardGridViewModel& m_ViewModel;

    QObject* m_ClickStart{ nullptr };
    SelectableCard* m_Selected{ nullptr };

    struct Card
    {
        SelectableCard* m_Widget;
        QString m_NameLowercase;
    };
    std::vector<Card> m_Cards;
    std::vector<QWidget*> m_Dummies;

    static inline constexpr uint32_t c_Columns{ 6 };
    uint32_t m_Rows;
};
