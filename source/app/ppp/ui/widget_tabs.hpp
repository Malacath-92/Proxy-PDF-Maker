#pragma once

#include <QTabWidget>

class Project;

class ActionsWidget;
class CardArea;
class PrintPreview;

class MainTabs : public QTabWidget
{
  public:
    MainTabs(ActionsWidget* actions,
             CardArea* card_area,
             PrintPreview* print_preview);

    int MaximumColumnsFromAvailableWidth(int available_width) const;

  private:
    CardArea* m_CardArea{ nullptr };
};
