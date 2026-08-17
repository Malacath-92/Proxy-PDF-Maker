#pragma once

#include <ppp/config_types.hpp>

#include <ppp/ui/popups/popups.hpp>

class QTableWidget;

class PaperSizePopup : public PopupBase
{
    Q_OBJECT

  public:
    PaperSizePopup(QWidget* parent,
                   const PageSizes& page_sizes,
                   const PageSizes& default_page_sizes);
    ~PaperSizePopup();

    virtual void showEvent(QShowEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

  signals:
    void PageSizesChanged(const PageSizes& page_sizes);

  protected:
    virtual QByteArray GetGeometry() override;
    virtual void RestoreGeometry(const QByteArray& geometry) override;

  private:
    void Apply();

    int ComputeColumnsWidth();
    int ComputeTableWidth();
    int ComputeWidthToCoverTable();
    void AutoResizeToTable();

    QTableWidget* m_Table{ nullptr };
    bool m_BlockResizeEvents{ false };
};
