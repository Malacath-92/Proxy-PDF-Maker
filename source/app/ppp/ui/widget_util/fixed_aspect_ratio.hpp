#pragma once

#include <QLayout>
#include <QWidget>

#include <ppp/util/log.hpp>

class FixedAspectRatioLayout : public QLayout
{
  public:
    FixedAspectRatioLayout(float width_over_height, QWidget* parent = nullptr)
        : QLayout{ parent }
        , m_AspectRatio{ width_over_height }
    {
    }
    virtual ~FixedAspectRatioLayout() override
    {
        delete m_Item;
    }

    virtual void addItem(QLayoutItem* item) override
    {
        if (m_Item != nullptr)
        {
            LogWarning("FixedAspectRatioLayout: Only one child allowed, existing child will be lost...");
            delete m_Item;
        }
        m_Item = item;
    }
    virtual int count() const override
    { return m_Item != nullptr ? 1 : 0; }
    virtual QLayoutItem* itemAt(int i) const override
    { return i == 0 ? m_Item : nullptr; }
    virtual QLayoutItem* takeAt(int i) override
    {
        if (i == 0 || m_Item == nullptr)
        {
            return nullptr;
        }
        QLayoutItem* item{ m_Item };
        m_Item = nullptr;
        return item;
    }

    virtual Qt::Orientations expandingDirections() const override
    { return Qt::Horizontal | Qt::Vertical; }

    virtual QSize sizeHint() const override
    { return WithMargins(Grown(m_Item != nullptr ? m_Item->sizeHint() : QSize{})); }
    virtual QSize minimumSize() const override
    { return WithMargins(Grown(m_Item != nullptr ? m_Item->minimumSize() : QSize{})); }

    virtual void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        if (m_Item == nullptr)
        {
            return;
        }

        const QRect available{ rect.marginsRemoved(contentsMargins()) };
        int width{ available.width() };
        int height{ qRound(width / m_AspectRatio) };
        if (height > available.height())
        {
            height = available.height();
            width = qRound(height * m_AspectRatio);
        }

        const QSize max{ m_Item->maximumSize() };
        if (width > max.width())
        {
            width = max.width();
            height = qRound(width / m_AspectRatio);
        }
        if (height > max.height())
        {
            height = max.height();
            width = qRound(height * m_AspectRatio);
        }

        m_Item->setGeometry(QRect{
            available.x() + (available.width() - width) / 2,
            available.y() + (available.height() - height) / 2,
            width,
            height,
        });
    }

  private:
    QSize Grown(QSize size) const
    {
        const int width{ qMax(size.width(), qRound(size.height() * m_AspectRatio)) };
        return QSize{ width, qRound(width / m_AspectRatio) };
    }
    QSize WithMargins(QSize size) const
    {
        const auto margins{ contentsMargins() };
        return size + QSize{
            margins.left() + margins.right(),
            margins.top() + margins.bottom(),
        };
    }

    QLayoutItem* m_Item{ nullptr };
    float m_AspectRatio;
};

class FixedAspectRatioContainer : public QWidget
{
  public:
    FixedAspectRatioContainer(float aspect_ratio, QWidget* child)
    {
        auto* layout{ new FixedAspectRatioLayout{ aspect_ratio, this } };
        if (child != nullptr)
        {
            layout->addWidget(child);
        }
        layout->setContentsMargins(0, 0, 0, 0);
    }
};
