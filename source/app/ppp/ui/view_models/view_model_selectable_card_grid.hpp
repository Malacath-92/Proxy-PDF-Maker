#pragma once

#include <span>

#include <QObject>

#include <ppp/project/project_types.hpp>

class CardViewModel;

class Project;

class SelectableCardGridViewModel : public QObject
{
    Q_OBJECT

  public:
    SelectableCardGridViewModel(const Project& project,
                                std::span<const fs::path> ignored_images = {});

    virtual bool HasCards() const;
    virtual bool HasIgnoredCards() const;

    virtual bool IsCardIgnored(const fs::path& card_name) const;

    virtual CardViewModel* MakeCardViewModel(const fs::path& card_name);

    virtual const CardContainer& GetCards() const;

  signals:
    // forward

    void CardAdded(const fs::path& card_name);

  protected:
    const Project& m_Project;
    std::span<const fs::path> m_IgnoredImages;
};
