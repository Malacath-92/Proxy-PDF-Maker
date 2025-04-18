#pragma once

#include <QLabel>
#include <QStackedWidget>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

class Project;
struct ImagePreview;

class CardImage : public QLabel
{
    Q_OBJECT

  public:
    struct Params
    {
        bool RoundedCorners{ true };
        Image::Rotation Rotation{ Image::Rotation::None };
        Length BleedEdge{ 0_mm };
        Pixel MinimumWidth{ 130_pix };
    };

    CardImage(const fs::path& image_name, const Project& project, Params params);

    void Refresh(const fs::path& image_name, const Project& project, Params params);

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }
    virtual int heightForWidth(int width) const override;

  private slots:
    void PreviewUpdated(const fs::path& image_name, const ImagePreview& preview);

  private:
    QPixmap FinalizePixmap(const QPixmap& pixmap);

    fs::path ImageName;
    Params OriginalParams;

    bool Rotated;
    Size CardSize;
    Length FullBleed;
    float CardRatio;
    Length BleedEdge;
    Length CornerRadius;

    QWidget* Spinner{ nullptr };
};

class BacksideImage : public CardImage
{
  public:
    BacksideImage(const fs::path& backside_name, const Project& project);
    BacksideImage(const fs::path& backside_name, Pixel minimum_width, const Project& project);

    void Refresh(const fs::path& backside_name, const Project& project);
    void Refresh(const fs::path& backside_name, Pixel minimum_width, const Project& project);
};

class StackedCardBacksideView : public QStackedWidget
{
    Q_OBJECT

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
