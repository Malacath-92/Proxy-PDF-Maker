#include <ppp/ui/widget_print_preview_card.hpp>

#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

PrintPreviewCardImage::PrintPreviewCardImage(const fs::path& card_name,
                                             const Project& project,
                                             CardImageWidgetParams params,
                                             size_t idx,
                                             QWidget* companion,
                                             std::optional<ClipRect> clip_rect,
                                             Size widget_size)
    : CardImage{
        card_name,
        project,
        params,
    }
    , m_Index{ idx }
    , m_Companion{ companion }
    , m_ClipRect{ clip_rect }
    , m_WidgetSize{ widget_size }
{
    setAcceptDrops(true);
}

void PrintPreviewCardImage::mousePressEvent(QMouseEvent* event)
{
    CardImage::mousePressEvent(event);

    if (!event->isAccepted() &&
        m_Companion->isVisible() &&
        event->button() == Qt::MouseButton::LeftButton)
    {
        QMimeData* mime_data{ new QMimeData };
        mime_data->setData(
            "data/size_t",
            QByteArray::fromRawData(reinterpret_cast<const char*>(&m_Index),
                                    sizeof(m_Index)));

        QDrag drag{ this };
        drag.setMimeData(mime_data);
        drag.setPixmap(pixmap().scaledToWidth(width() / 2));

        DragStarted();
        Qt::DropAction drop_action{ drag.exec() };
        (void)drop_action;
        DragFinished();

        event->accept();
    }
}

void PrintPreviewCardImage::dropEvent(QDropEvent* event)
{
    const auto raw_data{ event->mimeData()->data("data/size_t") };
    const auto other_index{ *reinterpret_cast<const size_t*>(raw_data.constData()) };
    if (other_index != m_Index)
    {
        ReorderCards(other_index, m_Index);
        event->acceptProposedAction();
    }
}

void PrintPreviewCardImage::enterEvent(QEnterEvent* event)
{
    CardImage::enterEvent(event);
    OnEnter();
}

void PrintPreviewCardImage::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("data/size_t"))
    {
        event->acceptProposedAction();
    }

    CardImage::dragEnterEvent(event);
    OnEnter();
}

void PrintPreviewCardImage::leaveEvent(QEvent* event)
{
    CardImage::leaveEvent(event);
    OnLeave();
}

void PrintPreviewCardImage::dragLeaveEvent(QDragLeaveEvent* event)
{
    CardImage::dragLeaveEvent(event);
    OnLeave();
}

void PrintPreviewCardImage::paintEvent(QPaintEvent* event)
{
    if (m_ClipRect.has_value())
    {
        const auto clipped_rect{ event->rect().intersected(GetClippedRect()) };
        const auto scaled_pixmap{
            [this]()
            {
                const auto unscaled_pixmap{ pixmap() };
                const auto pdpr{ unscaled_pixmap.devicePixelRatio() };
                const auto dpr{ devicePixelRatio() };
                const bool scaled_contents{ hasScaledContents() };
                if (scaled_contents || dpr != pdpr)
                {
                    const auto scaled_size{
                        scaled_contents ? (contentsRect().size() * dpr)
                                        : (unscaled_pixmap.size() * (dpr / pdpr))
                    };
                    if (!m_ScaledPixmap.has_value() || m_ScaledPixmap.value().size() != scaled_size)
                    {
                        m_ScaledPixmap = unscaled_pixmap.scaled(scaled_size,
                                                                Qt::IgnoreAspectRatio,
                                                                Qt::SmoothTransformation);
                        m_ScaledPixmap.value().setDevicePixelRatio(dpr);
                    }
                    return m_ScaledPixmap.value();
                }
                else
                {
                    return pixmap();
                }
            }()
        };

        QPainter painter{ this };
        painter.setClipRect(clipped_rect);
        painter.drawPixmap(QPoint{ 0, 0 }, scaled_pixmap);
        painter.end();
    }
    else
    {
        CardImage::paintEvent(event);
    }
}

QRect PrintPreviewCardImage::GetClippedRect() const
{
    if (m_ClipRect.has_value())
    {
        const dla::ivec2 pixel_size{
            size().width(),
            size().height(),
        };
        const auto pixel_ratio{ pixel_size / m_WidgetSize };
        const auto pixel_clip_offset{ m_ClipRect.value().m_Position * pixel_ratio };
        const auto pixel_clip_size{ m_ClipRect.value().m_Size * pixel_ratio };
        const QRect clipped_rect{
            QPoint{ (int)pixel_clip_offset.x, (int)pixel_clip_offset.y },
            QSize{ (int)pixel_clip_size.x, (int)pixel_clip_size.y },
        };
        return clipped_rect;
    }
    else
    {
        return rect();
    }
}

void PrintPreviewCardImage::OnEnter()
{
    {
        const auto clipped_rect{ GetClippedRect() };
        m_Companion->move(pos() +
                          clipped_rect.topLeft() -
                          QPoint{ c_CompanionSizeDelta, c_CompanionSizeDelta });
        m_Companion->resize(clipped_rect.size() +
                            2 * QSize{ c_CompanionSizeDelta, c_CompanionSizeDelta });
    }
    m_Companion->setVisible(true);
    m_Companion->raise();
    raise();
}
void PrintPreviewCardImage::OnLeave()
{
    m_Companion->setVisible(false);
}
