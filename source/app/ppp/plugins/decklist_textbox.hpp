#pragma once

#include <QTextEdit>

class DecklistTextEdit : public QTextEdit
{
    virtual void insertFromMimeData(const QMimeData* source) override;

    virtual void paintEvent(QPaintEvent* event) override;
};
