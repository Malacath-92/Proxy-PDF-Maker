#pragma once

#include <QTabWidget>

class Project;

class CardArea;
class PrintPreview;

class MainTabs : public QTabWidget
{
  public:
    MainTabs(CardArea* card_area, PrintPreview* print_preview);

    int MaximumColumnsFromAvailableWidth(int available_width) const;

  private:
    CardArea* m_CardArea{ nullptr };
};
