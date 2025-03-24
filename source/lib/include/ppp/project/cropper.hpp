#pragma once

#include <atomic>

#include <QObject>
#include <QTimer>

#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

class QThread;

class Cropper : public QObject
{
    Q_OBJECT;

  public:
    Cropper(const Project& project);
    ~Cropper();

    void Start();

  signals:
    void CropWorkStart();
    void CropWorkDone();

    void PreviewWorkStart();
    void PreviewWorkDone();

    void PreviewUpdated(const fs::path& card_name, const ImagePreview& preview);

  public slots:
    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

  private:
    void CropWork();
    void PreviewWork();

  private:
    void PushWork(const fs::path& card_name);
    void RemoveWork(const fs::path& card_name);

    // These do the actual work, return false when no work to do
    template<class T>
    bool DoCropWork(T* signaller);
    template<class T>
    bool DoPreviewWork(T* signaller);

    std::mutex PendingCropWorkMutex;
    std::vector<fs::path> PendingCropWork;

    std::mutex PendingPreviewWorkMutex;
    std::vector<fs::path> PendingPreviewWork;

    std::shared_mutex PropertyMutex;
    fs::path ImageDir;
    fs::path CropDir;

    class CropperSignalRouter* Router;

    QThread* CropThread;
    std::atomic_bool CropDone{ true };

    QThread* PreviewThread;
    std::atomic_bool PreviewDone{ true };

    std::atomic_bool Quit{ false };

    std::atomic_uint32_t ThreadsDone{ 0 };

    QTimer* CropTimer;
    QTimer* PreviewTimer;

    const Project& TheProject;
};
