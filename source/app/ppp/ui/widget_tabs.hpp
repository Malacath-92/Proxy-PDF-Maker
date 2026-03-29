#pragma once

#include <QTabWidget>

class Project;

class CardArea;
class PrintPreview;

class MainTabs : public QTabWidget
{
  public:
    MainTabs(CardArea* card_area, PrintPreview* print_preview);
};
