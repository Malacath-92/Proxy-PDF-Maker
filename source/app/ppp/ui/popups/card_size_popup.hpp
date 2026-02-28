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

  protected:
    virtual QByteArray GetGeometry() override;
    virtual void RestoreGeometry(const QByteArray& geometry) override;

  private:
    void Apply();

    static int ComputeColumnsWidth(QTableWidget* table);
    static int ComputeAuxWidth(QTableWidget* table);
    static int ComputeTableWidth(QTableWidget* table);
    int ComputeWidthToCoverTable(QTableWidget* table) const;
    void AutoResizeToTable(QTableWidget* table);
    void AutoResizeToTables();

    QTableWidget* m_RectTable{ nullptr };
    bool m_BlockResizeEventsRectTable{ false };

    QTableWidget* m_SvgTable{ nullptr };
    bool m_BlockResizeEventsSvgTable{ false };
};
