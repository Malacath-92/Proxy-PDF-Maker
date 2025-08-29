#pragma once

#include <ppp/config.hpp>

#include <ppp/ui/popups.hpp>

class QTableWidget;

class CardSizePopup : public PopupBase
{
    Q_OBJECT

  public:
    CardSizePopup(QWidget* parent, const Config& config);
    ~CardSizePopup();

    virtual void showEvent(QShowEvent* event) override;

  signals:
    void CardSizesChanged(const std::map<std::string, Config::CardSizeInfo>& card_sizes);

  private:
    void Apply();

    QTableWidget* m_Table{ nullptr };
};
