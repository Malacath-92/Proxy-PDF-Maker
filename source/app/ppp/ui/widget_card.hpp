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
        bool m_RoundedCorners{ true };
        Image::Rotation m_Rotation{ Image::Rotation::None };
        Length m_BleedEdge{ 0_mm };
        Pixel m_MinimumWidth{ 130_pix };
    };

    CardImage(const fs::path& card_name, const Project& project, Params params);

    void Refresh(const fs::path& card_name, const Project& project, Params params);

    void RotateImageLeft();
    void RotateImageRight();

    const fs::path& GetCardName() const
    {
        return m_CardName;
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }
    virtual int heightForWidth(int width) const override;

  private slots:
    void PreviewUpdated(const fs::path& card_name, const ImagePreview& preview);

  private:
    QPixmap FinalizePixmap(const QPixmap& pixmap);
    void AddBadFormatWarning(const ImagePreview& preview);
    void ClearChildren();

    fs::path m_CardName;
    Params m_OriginalParams;

    bool m_Rotated;
    Size m_CardSize;
    Length m_FullBleed;
    float m_CardRatio;
    Length m_BleedEdge;
    Length m_CornerRadius;

    QWidget* m_Warning{ nullptr };
    QWidget* m_Spinner{ nullptr };
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
    StackedCardBacksideView(CardImage* image, QWidget* backside);

    void RefreshBackside(QWidget* new_backside);

    void RotateImageLeft();
    void RotateImageRight();

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

    CardImage* m_Image;
    QWidget* m_Backside;
    QWidget* m_BacksideContainer;
};
