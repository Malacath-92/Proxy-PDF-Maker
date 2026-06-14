#pragma once

#include <QTextEdit>

#include <ppp/ui/popups.hpp>

class QTextEdit;
class QCompleter;

class Project;

class LocalDecklistTextEdit : public QTextEdit
{
    Q_OBJECT;

  public:
    LocalDecklistTextEdit(const Project& project);
    LocalDecklistTextEdit(const Project& project, const QString& text);

    bool CompleterActive() const;

  private:
    virtual void keyPressEvent(QKeyEvent* e) override;

    QCompleter* MakeCompleter(const Project& project);

    QString TextUnderCursor() const;
    void Complete(const QString& completion);

    QCompleter* m_Completer{ nullptr };
};

class DecklistPopup : public PopupBase
{
    Q_OBJECT

  public:
    DecklistPopup(QWidget* parent, const Project& project);
    ~DecklistPopup();

  signals:
    void DecklistChanged(const std::unordered_map<fs::path, uint32_t>& decklist);

  private:
    virtual void keyPressEvent(QKeyEvent* e) override;

    void Apply();

    LocalDecklistTextEdit* m_Text{ nullptr };
};
