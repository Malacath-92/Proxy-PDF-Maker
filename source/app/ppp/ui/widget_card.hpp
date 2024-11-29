#pragma once

#include <QLabel>
#include <QStackedWidget>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

struct Project;

class CardImage : public QLabel
{
  public:
    struct Params
    {
        bool RoundedCorners{ true };
        Image::Rotation Rotation{ Image::Rotation::None };
    };

    CardImage(const Image& image, Params params);

    void Refresh(const Image& image, Params params);

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }
    virtual int heightForWidth(int width) const override;

  private:
    bool Rotated;
};

class BacksideImage : public CardImage
{
  public:
    BacksideImage(const fs::path& backside_name, const Project& project);

    void Refresh(const fs::path& backside_name, const Project& project);
};

class StackedCardBacksideView : public QStackedWidget
{
    Q_OBJECT;

  public:
    StackedCardBacksideView(QWidget* image, QWidget* backside);

    void RefreshBackside(QWidget* new_backside);

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }
    virtual int heightForWidth(int width) const override;

  signals:
    void BacksideReset();
    void BacksideClicked();

  private:
    void RefreshSizes(QSize size);

    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    QWidget* Image;
    QWidget* Backside;
    QWidget* BacksideContainer;
};
