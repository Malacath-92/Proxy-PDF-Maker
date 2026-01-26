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
    virtual void resizeEvent(QResizeEvent* event) override;

  signals:
    void CardSizesChanged(const std::map<std::string, Config::CardSizeInfo>& card_sizes);

  private:
    void Apply();

    int ComputeColumnsWidth();
    int ComputeTableWidth();
    int ComputeWidthToCoverTable();
    void AutoResizeToTable();

    QTableWidget* m_Table{ nullptr };
    bool m_BlockResizeEvents{ false };
};
