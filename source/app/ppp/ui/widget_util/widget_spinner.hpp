#pragma once

#include <QSvgWidget>

class SpinnerWidget : public QSvgWidget
{
  public:
    SpinnerWidget()
        : QSvgWidget{ ":/res/spinner.svg" }
    {
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        return width;
    }
};
