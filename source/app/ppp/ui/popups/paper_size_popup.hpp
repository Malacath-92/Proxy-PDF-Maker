#pragma once

#include <ppp/config.hpp>

#include <ppp/ui/popups.hpp>

class QTableWidget;

class PaperSizePopup : public PopupBase
{
    Q_OBJECT

  public:
    PaperSizePopup(QWidget* parent, const Config& config);
    ~PaperSizePopup();

    virtual void showEvent(QShowEvent* event) override;

  signals:
    void PageSizesChanged(const std::map<std::string, Config::SizeInfo>& page_sizes);

  private:
    void Apply();

    const Config& m_Config;

    QTableWidget* m_Table{ nullptr };
};
