#pragma once

#include <any>
#include <memory>
#include <optional>

#include <QRegularExpression>
#include <QTemporaryDir>

#include <ppp/ui/popups.hpp>

class QTextEdit;
class QProgressBar;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;

enum class InputType
{
    Decklist,
    MPCAutofill,
    None,
};

class MtgDownloaderPopup : public PopupBase
{
  public:
    MtgDownloaderPopup(QWidget* parent);
    ~MtgDownloaderPopup();

  private slots:
    void MPCFillRequestFinished(QNetworkReply* reply);

  private:
    void DoDownload();

    void InstallLogHook();
    void UninstallLogHook();

    static InputType StupidInferSource(const QString& text);

    inline static const QRegularExpression g_DecklistRegex{
        R"'((\d+)x? "?(.*)"? \(([^ ]+)\) ((.*-)?\d+))'"
    };

    InputType m_InputType{ InputType::None };

    QTextEdit* m_TextInput{ nullptr };
    QLabel* m_Hint{ nullptr };
    QProgressBar* m_ProgressBar{ nullptr };
    QPushButton* m_DownloadButton{ nullptr };

    QTemporaryDir m_OutputDir{};

    std::optional<uint32_t> m_LogHookId{ std::nullopt };

    std::any m_CardsData;
    size_t m_NumDownloads{ 0 };

    std::unique_ptr<QNetworkAccessManager> m_NetworkManager;
};

class SelectInputTypePopup : public PopupBase
{
  public:
    SelectInputTypePopup(QWidget* parent);

    InputType GetInputType() const
    {
        return m_InputType;
    }

  private:
    InputType m_InputType{ InputType::None };
};
