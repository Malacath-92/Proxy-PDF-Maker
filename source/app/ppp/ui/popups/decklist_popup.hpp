#pragma once

#include <QTextEdit>

#include <ppp/ui/popups/popups.hpp>

class QTextEdit;
class QCompleter;
class QSyntaxHighlighter;

class Project;
class DecklistPopupViewModel;

class LocalDecklistTextEdit : public QTextEdit
{
    Q_OBJECT

  public:
    LocalDecklistTextEdit(const DecklistPopupViewModel& view_model);
    LocalDecklistTextEdit(const DecklistPopupViewModel& view_model, const QString& text);

    bool CompleterActive() const;

  private:
    virtual bool event(QEvent* e) override;
    virtual void keyPressEvent(QKeyEvent* e) override;

    QCompleter* MakeCompleter(const DecklistPopupViewModel& view_model);

    QString TextUnderCursor() const;
    void Complete(const QString& completion);

    QCompleter* m_Completer{ nullptr };
    QSyntaxHighlighter* m_Highlighter{ nullptr };
};

class DecklistPopup : public PopupBase
{
    Q_OBJECT

  public:
    DecklistPopup(DecklistPopupViewModel* view_model,
                  QWidget* parent = nullptr);
    ~DecklistPopup();

  signals:
    void DecklistChanged(const std::unordered_map<fs::path, uint32_t>& decklist);

  private:
    virtual void keyPressEvent(QKeyEvent* e) override;

    void Apply();

    LocalDecklistTextEdit* m_Text{ nullptr };
};
