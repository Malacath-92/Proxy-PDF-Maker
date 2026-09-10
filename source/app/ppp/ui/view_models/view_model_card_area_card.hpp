#pragma once

#include <QObject>

#include <ppp/project/project_types.hpp>
#include <ppp/util.hpp>

class Project;

class CardViewModel;
class BlankCardViewModel;
class ImageBrowseViewModel;

class CardAreaCardViewModel : public QObject
{
    Q_OBJECT

    friend class CardArea;
    friend class CardAreaCardWidget;

  public:
    CardAreaCardViewModel(const fs::path& card_name,
                          Project& project);

    const fs::path& GetCardName() const;
    bool HasBackside() const;
    bool IsBacksideEnabled() const;

    CardViewModel* MakeCardViewModel() const;
    CardViewModel* MakeBacksideCardViewModel() const;
    BlankCardViewModel* MakeBlankCardViewModel() const;
    ImageBrowseViewModel* MakeImageBrowseViewModel() const;

  signals:
    // forward

    void BacksideEnabledChanged(bool backside_enabled);

    void CardCountChanged(uint32_t count);
    void CardBacksideShortEdgeChanged(bool card_backside_short_edge);

    void CardBacksideChanged(OptionalImageRef backside);

  private slots:
    void DecrementCard();
    void IncrementCard();
    void SetCardCount(const QString& count);
    void SetCardBacksideShortEdge(Qt::CheckState backside_short_edge);

    void SetBacksideImage(const fs::path& backside_name);
    void ClearBacksideImage();
    void SetBacksideImageDefault();

  private:
    void EmitDefaults();

    fs::path m_CardName;
    Project& m_Project;
};
