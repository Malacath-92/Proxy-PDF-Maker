#pragma once

#include <QTabWidget>

struct Project;

class CardScrollArea;
class PrintPreview;

class MainTabs : public QTabWidget
{
  public:
    MainTabs(Project& project, CardScrollArea* scroll_area, PrintPreview* print_preview);
};
