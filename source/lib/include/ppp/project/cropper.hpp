#pragma once

#include <atomic>
#include <chrono>
#include <functional>

#include <QObject>
#include <QTemporaryDir>
#include <QTimer>

#include <ppp/util.hpp>

#include <ppp/project/image_database.hpp>
#include <ppp/project/project.hpp>

class QThread;

class Cropper : public QObject
{
    Q_OBJECT

  public:
    Cropper(std::function<const cv::Mat*(std::string_view)> get_color_cube, const Project& project);
    ~Cropper();

    void Start();

    void ClearCropWork();
    void ClearPreviewWork();

  signals:
    void CropWorkStart();
    void CropWorkDone();

    void PreviewWorkStart();
    void PreviewWorkDone();

    void CropProgress(float progress);
    void PreviewUpdated(const fs::path& card_name, const ImagePreview& preview);

    void RestartTimers(QPrivateSignal);

  public slots:
    void NewProjectOpenedDiff(const Project::ProjectData& data);
    void ImageDirChangedDiff(const fs::path& image_dir,
                             const fs::path& crop_dir,
                             const fs::path& uncrop_dir,
                             const std::vector<fs::path>& loaded_previews);
    void CardSizeChangedDiff(std::string card_size);
    void BleedChangedDiff(Length bleed);
    void ColorCubeChangedDiff(const std::string& cube_name);
    void BasePreviewWidthChangedDiff(Pixel base_preview_width);
    void MaxDPIChangedDiff(PixelDensity dpi);

    void CardAdded(const fs::path& card_name, bool needs_crop, bool needs_preview);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

    void PauseWork();
    void RestartWork();

  private:
    void CropWork();
    void PreviewWork();

    void PushWork(const fs::path& card_name, bool needs_crop, bool needs_preview);
    void RemoveWork(const fs::path& card_name);

    // These do the actual work, return false when no work to do
    template<class T>
    bool DoCropWork(T* signaller);
    template<class T>
    bool DoPreviewWork(T* signaller);

    std::function<const cv::Mat*(std::string_view)> m_GetColorCube;

    std::shared_mutex m_ImageDBMutex;
    ImageDataBase m_ImageDB;

    std::mutex m_PendingCropWorkMutex;
    std::vector<fs::path> m_PendingCropWork;
    std::atomic_uint32_t m_TotalWorkDone{};

    std::mutex m_PendingPreviewWorkMutex;
    std::vector<fs::path> m_PendingPreviewWork;

    std::shared_mutex m_PropertyMutex;
    Project::ProjectData m_Data;
    Config m_Cfg;
    std::vector<fs::path> m_LoadedPreviews;

    using time_point = decltype(std::chrono::high_resolution_clock::now());
    time_point m_CropWorkStartPoint;

    class CropperSignalRouter* m_Router;

    // We give the cropper a few updates before triggering done
    // so we don't ping-pong start<->done when we work fast
    static inline constexpr uint32_t c_UpdatesBeforeDoneTrigger{ 6 };

    QThread* m_CropThread;
    std::atomic_uint32_t m_CropDone{ 0 };

    QThread* m_PreviewThread;
    std::atomic_uint32_t m_PreviewDone{ 0 };

    std::atomic_bool m_Pause{ false };
    std::atomic_uint32_t m_ThreadsPaused{ 0 };

    std::atomic_bool m_Quit{ false };
    std::atomic_uint32_t m_ThreadsDone{ 0 };

    QTimer* m_CropTimer;
    QTimer* m_PreviewTimer;
};
