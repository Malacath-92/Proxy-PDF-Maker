#include <ppp/plugins/decklist_textbox.hpp>

#include <QFile>
#include <QMimeData>
#include <QPainter>
#include <QUrl>

void DecklistTextEdit::insertFromMimeData(const QMimeData* source)
{
    if (source->hasText())
    {
        const auto url{ QUrl::fromUserInput(source->text()) };
        if (url.isValid() && url.isLocalFile() && QFile::exists(url.toLocalFile()))
        {
            QFile file{ url.toLocalFile() };
            if (file.open(QFile::OpenModeFlag::ReadOnly))
            {
                insertPlainText(file.readAll());
                return;
            }
        }
    }

    QTextEdit::insertFromMimeData(source);
}

void DecklistTextEdit::paintEvent(QPaintEvent* event)
{
    const auto placeholder{ placeholderText() };
    setPlaceholderText("");
    QTextEdit::paintEvent(event);
    setPlaceholderText(placeholder);

    if (document()->isEmpty() && !placeholder.isEmpty())
    {
        QPainter painter{ viewport() };

        QColor placeholder_color{ palette().text().color() };
        placeholder_color.setAlpha(128);
        painter.setPen(placeholder_color);

        const auto margin{ static_cast<int>(document()->documentMargin()) };
        const auto target_rect{ viewport()->rect().adjusted(margin + 2, margin, -margin, -margin) };

        const auto flags{ Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap };
        painter.drawText(target_rect, flags, placeholderText());
    }
}
