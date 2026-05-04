#pragma once

#include <QFile>
#include <QMimeData>
#include <QTextEdit>
#include <QUrl>

class DecklistTextEdit : public QTextEdit
{
    virtual void insertFromMimeData(const QMimeData* source) override
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
};
