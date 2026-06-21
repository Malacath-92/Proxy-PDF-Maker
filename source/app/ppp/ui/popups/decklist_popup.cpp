#include <ppp/ui/popups/decklist_popup.hpp>

#include <ranges>

#include <QApplication>

#include <QAbstractItemView>
#include <QCompleter>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QStringListModel>
#include <QSyntaxHighlighter>
#include <QTextEdit>
#include <QToolTip>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

template<class FunT>
static auto ForEachCardInList(const QString& decklist,
                              FunT&& fun)
{
    const auto lines{
        decklist.split(QRegularExpression{ "[\r\n]" }, Qt::SplitBehaviorFlags::SkipEmptyParts)
    };
    for (const auto& line : lines)
    {
        const auto first_space{ line.indexOf("x ") };
        const auto amount{ line.mid(0, first_space).toUInt() };
        auto card_name{ line.mid(first_space + 2).trimmed() };
        fun(std::move(card_name), amount);
    }
}

class CardNameModel : public QAbstractListModel
{
  public:
    CardNameModel(const Project& project)
        : m_Strings{ project.m_Data.m_Cards |
                     std::views::filter(std::not_fn(&CardInfo::m_Transient)) |
                     std::views::filter(std::not_fn(&CardInfo::m_Hidden)) |
                     std::views::transform(&CardInfo::m_Name) |
                     std::views::transform(&fs::path::filename) |
                     std::views::transform(QOverload<const fs::path&>::of(&ToQString)) |
                     std::ranges::to<QList>() }
    {
    }

    int rowCount(const QModelIndex& /*parent*/ = QModelIndex()) const override
    {
        return static_cast<int>(m_Strings.size());
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (index.isValid() && index.row() < m_Strings.size())
        {
            if (role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return m_Strings[index.row()];
            }
        }
        return QVariant{};
    }

    void DecklistChanged(const QString& decklist)
    {
        QList<QString> strings{ m_Strings + m_BlockedStrings };
        QList<QString> blocked_string{};
        ForEachCardInList(decklist,
                          [&](QString card_name, uint32_t /*amount*/)
                          {
                              if (strings.contains(card_name))
                              {
                                  strings.removeAll(card_name);
                                  blocked_string.append(std::move(card_name));
                              }
                              else
                              {
                                  for (auto it{ strings.begin() }; it != strings.end(); ++it)
                                  {
                                      const auto stem{ it->mid(it->lastIndexOf(".")) };
                                      if (stem == card_name)
                                      {
                                          blocked_string.append(std::move(*it));
                                          strings.erase(it);
                                      }
                                  }
                              }
                          });

        beginResetModel();
        m_Strings = std::move(strings);
        m_BlockedStrings = std::move(blocked_string);
        endResetModel();
    }

  private:
    QList<QString> m_Strings;
    QList<QString> m_BlockedStrings;
};

class DecklistHighlighter : public QSyntaxHighlighter
{
  public:
    DecklistHighlighter(QTextDocument* parent,
                        const Project& project)
        : QSyntaxHighlighter{ parent }
        , m_Project{ project }
    {
    }

    virtual void highlightBlock(const QString& text) override
    {
        const QRegularExpression number_re{ "^\\d+x" };
        const auto number_match{ number_re.match(text) };
        if (number_match.isValid() && number_match.capturedLength() > 0)
        {
            QTextCharFormat number_format{};
            number_format.setFontWeight(QFont::Bold);
            setFormat(static_cast<int>(number_match.capturedStart()),
                      static_cast<int>(number_match.capturedLength()) - 1,
                      number_format);

            const QRegularExpression card_re{ "[^\\s].*$" };
            const auto card_match{ card_re.match(text, number_match.capturedLength()) };
            if (card_match.isValid() && card_match.capturedLength() > 0)
            {
                QTextCharFormat card_format{};
                const auto card_name{ card_match.captured().toStdString() };
                if (m_Project.HasCard(card_name) || m_Project.HasCardByStem(card_name))
                {
                    card_format.setFontItalic(true);
                }
                else
                {
                    card_format.setBackground(Qt::darkRed);
                    card_format.setToolTip("This card is not part of the project");
                }
                setFormat(static_cast<int>(card_match.capturedStart()),
                          static_cast<int>(card_match.capturedLength()),
                          card_format);
            }
        }
        else
        {
            QTextCharFormat error_format{};
            error_format.setBackground(Qt::darkRed);
            error_format.setToolTip("The line has to start with the amount, e.g. \"1x Card.png\"");
            setFormat(0,
                      static_cast<int>(text.length()),
                      error_format);
        }
    }

  private:
    const Project& m_Project;
};

LocalDecklistTextEdit::LocalDecklistTextEdit(const Project& project)
    : QTextEdit{}
    , m_Completer{ MakeCompleter(project) }
    , m_Highlighter{ new DecklistHighlighter{ document(), project } }
{
    setMouseTracking(true);
}
LocalDecklistTextEdit::LocalDecklistTextEdit(const Project& project, const QString& text)
    : QTextEdit{ text }
    , m_Completer{ MakeCompleter(project) }
    , m_Highlighter{ new DecklistHighlighter{ document(), project } }
{
    setMouseTracking(true);
}

bool LocalDecklistTextEdit::CompleterActive() const
{
    return m_Completer->popup() != nullptr &&
           m_Completer->popup()->isVisible();
}

bool LocalDecklistTextEdit::event(QEvent* e)
{
    if (e->type() == QEvent::Type::ToolTip)
    {
        const auto* help_e{ static_cast<QHelpEvent*>(e) };
        const auto cursor{ cursorForPosition(help_e->pos()) };
        const auto block{ cursor.block() };
        const auto formats{ block.layout()->formats() };
        const auto relative_pos{ cursor.position() - block.position() };

        for (const auto& range : formats)
        {
            if (relative_pos >= range.start &&
                relative_pos < (range.start + range.length))
            {
                const auto& format{ range.format };
                if (format.hasProperty(QTextFormat::Property::TextToolTip))
                {
                    const auto tooltip_text{ format.toolTip() };
                    if (!tooltip_text.isEmpty())
                    {
                        QToolTip::showText(help_e->globalPos(), tooltip_text, this);
                        return true;
                    }
                }
            }
        }
    }

    QToolTip::hideText();

    return QTextEdit::event(e);
}

void LocalDecklistTextEdit::keyPressEvent(QKeyEvent* e)
{
    if (m_Completer == nullptr)
    {
        QTextEdit::keyPressEvent(e);
        return;
    }

    const bool completer_active{ CompleterActive() };
    if (completer_active)
    {
        switch (e->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        case Qt::Key_Down:
        case Qt::Key_Up:
            // The completer handles these keys while active
            e->ignore();
            return;
        default:
            break;
        }
    }

    const auto request_completion{ e->key() == Qt::Key_Tab };
    const auto old_len{ document()->characterCount() };
    if (!request_completion)
    {
        QTextEdit::keyPressEvent(e);
    }

    const auto complete{
        [this]()
        {
            const auto completion_prefix{ TextUnderCursor() };
            if (!completion_prefix.isEmpty())
            {
                m_Completer->setCompletionPrefix(completion_prefix);

                auto* popup{ m_Completer->popup() };
                popup->setAttribute(Qt::WA_WindowPropagation, false);
                popup->setFocusPolicy(Qt::NoFocus);
                popup->setFocusProxy(this);
                popup->setCurrentIndex(m_Completer->completionModel()->index(0, 0));

                auto cr{ cursorRect() };
                cr.setWidth(popup->sizeHintForColumn(0) +
                            popup->verticalScrollBar()->sizeHint().width());
                m_Completer->complete(cr);
            }
            else if (m_Completer->popup()->isVisible())
            {
                m_Completer->popup()->hide();
            }
        }
    };

    if (request_completion ||
        (!e->text().isEmpty() &&
         document()->characterCount() > old_len))
    {
        complete();
    }
    else if (m_Completer->popup()->isVisible())
    {
        m_Completer->popup()->hide();
    }
}

QCompleter* LocalDecklistTextEdit::MakeCompleter(const Project& project)
{
    auto* completer{ new QCompleter{ this } };
    completer->setWidget(this);
    completer->setModelSorting(QCompleter::UnsortedModel);
    completer->setWrapAround(true);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);

    auto* model{ new CardNameModel{ project } };
    completer->setModel(model);

    QObject::connect(completer,
                     QOverload<const QString&>::of(&QCompleter::activated),
                     this,
                     &LocalDecklistTextEdit::Complete);
    QObject::connect(this,
                     &QTextEdit::textChanged,
                     model,
                     [this, model]()
                     { model->DecklistChanged(toPlainText()); });

    return completer;
}

QString LocalDecklistTextEdit::TextUnderCursor() const
{
    auto tc{ textCursor() };
    const auto column{ tc.columnNumber() };
    tc.select(QTextCursor::LineUnderCursor);
    const auto line{ tc.selectedText().sliced(0, column) };
    const auto first_space{ line.indexOf(' ') };
    return first_space == -1 || first_space == line.size()
               ? ""
               : line.sliced(first_space + 1).trimmed();
}

void LocalDecklistTextEdit::Complete(const QString& completion)
{
    auto tc{ textCursor() };
    const auto prefix{ m_Completer->completionPrefix().length() };
    tc.setPosition(tc.position() - static_cast<int>(prefix), QTextCursor::KeepAnchor);
    tc.insertText(completion + "\n");
    setTextCursor(tc);
}

DecklistPopup::DecklistPopup(QWidget* parent,
                             const Project& project)
    : PopupBase{ parent }
{
    m_AutoCenter = false;
    m_PersistGeometry = true;

    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("Amounts from Decklist");
    setObjectName("DecklistPopup");

    m_Text = new LocalDecklistTextEdit{ project };
    m_Text->setPlaceholderText("1x Sol Ring\n3x Arcane Signet");
    for (const auto& card : project.m_Data.m_Cards)
    {
        if (card.m_Num > 0 && card.m_Hidden == 0)
        {
            m_Text->append(
                QString{ "%1x %2" }
                    .arg(card.m_Num)
                    .arg(ToQString(card.m_Name.filename())));
        }
    }

    auto* ok_button{ new QPushButton{ "OK" } };
    auto* apply_button{ new QPushButton{ "Apply" } };
    auto* cancel_button{ new QPushButton{ "Cancel" } };

    auto* buttons_layout{ new QHBoxLayout };
    buttons_layout->setContentsMargins(0, 0, 0, 0);
    buttons_layout->addWidget(ok_button);
    buttons_layout->addWidget(apply_button);
    buttons_layout->addWidget(cancel_button);

    auto* buttons{ new QWidget };
    buttons->setLayout(buttons_layout);

    auto* outer_layout{ new QVBoxLayout };
    outer_layout->addWidget(m_Text);
    outer_layout->addWidget(buttons);
    outer_layout->addWidget(buttons);

    setLayout(outer_layout);

    QObject::connect(ok_button,
                     &QPushButton::clicked,
                     [this]()
                     {
                         Apply();
                         close();
                     });
    QObject::connect(apply_button,
                     &QPushButton::clicked,
                     this,
                     &DecklistPopup::Apply);
    QObject::connect(cancel_button,
                     &QPushButton::clicked,
                     this,
                     &QDialog::close);
}

DecklistPopup::~DecklistPopup()
{
}

void DecklistPopup::keyPressEvent(QKeyEvent* e)
{
    const bool completer_active{ m_Text->CompleterActive() };
    if (completer_active)
    {
        switch (e->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        case Qt::Key_Down:
        case Qt::Key_Up:
            // The completer handles these keys while active
            e->ignore();
            return;
        default:
            break;
        }
    }

    PopupBase::keyPressEvent(e);
}

void DecklistPopup::Apply()
{
    std::unordered_map<fs::path, uint32_t> decklist;
    ForEachCardInList(m_Text->toPlainText(),
                      [&](QString card_name, uint32_t amount)
                      {
                          decklist[fs::path{ card_name.toStdString() }] = amount;
                      });
    DecklistChanged(decklist);
}
