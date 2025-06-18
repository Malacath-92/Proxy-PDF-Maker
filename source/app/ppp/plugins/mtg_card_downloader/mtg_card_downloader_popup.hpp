#pragma once

#include <any>
#include <memory>
#include <optional>

#include <QTemporaryDir>
#include <QTimer>

#include <ppp/ui/popups.hpp>

class QCheckBox;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;

class Project;

enum class InputType
{
    Decklist,
    MPCAutofill,
    None,
};

class MtgDownloaderPopup : public PopupBase
{
  public:
    MtgDownloaderPopup(QWidget* parent, Project& project);
    ~MtgDownloaderPopup();

  private slots:
    void MPCFillRequestFinished(QNetworkReply* reply);

    void ScryfallRequestFinished(QNetworkReply* reply);

  private:
    void DoDownload();
    void FinalizeDownload();

    void InstallLogHook();
    void UninstallLogHook();

    static InputType StupidInferSource(const QString& text);

    Project& m_Project;

    InputType m_InputType{ InputType::None };

    QTextEdit* m_TextInput{ nullptr };
    QCheckBox* m_ClearCheckbox{ nullptr };
    QLabel* m_Hint{ nullptr };
    QProgressBar* m_ProgressBar{ nullptr };
    QPushButton* m_DownloadButton{ nullptr };

    QTemporaryDir m_OutputDir{};

    std::optional<uint32_t> m_LogHookId{ std::nullopt };

    std::any m_CardsData;

    std::unique_ptr<struct ScryfallState> m_ScryfallState;
    QTimer m_ScryfallTimer; // for padding requests on Scryfall's request

    size_t m_NumRequests{ 0 };
    size_t m_NumReplies{ 0 };

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
