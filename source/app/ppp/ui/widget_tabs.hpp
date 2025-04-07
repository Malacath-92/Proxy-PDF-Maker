#pragma once

#include <QTabWidget>

class Project;

class CardScrollArea;
class PrintPreview;

class MainTabs : public QTabWidget
{
  public:
    MainTabs(Project& project, CardScrollArea* scroll_area, PrintPreview* print_preview);
};
