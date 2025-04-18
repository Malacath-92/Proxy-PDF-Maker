#pragma once

#include <atomic>
#include <functional>

#include <QObject>
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

  public slots:
    void NewProjectOpenedDiff(const Project::ProjectData& data);
    void ImageDirChangedDiff(const fs::path& image_dir, const fs::path& crop_dir, const std::vector<fs::path>& loaded_previews);
    void CardSizeChangedDiff(std::string card_size);
    void BleedChangedDiff(Length bleed);
    void EnableUncropChangedDiff(bool enable_uncrop);
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

  private:
    void PushWork(const fs::path& card_name, bool needs_crop, bool needs_preview);
    void RemoveWork(const fs::path& card_name);

    // These do the actual work, return false when no work to do
    template<class T>
    bool DoCropWork(T* signaller);
    template<class T>
    bool DoPreviewWork(T* signaller);

    std::function<const cv::Mat*(std::string_view)> GetColorCube;

    std::shared_mutex ImageDBMutex;
    ImageDataBase ImageDB;

    std::mutex PendingCropWorkMutex;
    std::vector<fs::path> PendingCropWork;
    std::atomic_uint32_t TotalWorkDone{};

    std::mutex PendingPreviewWorkMutex;
    std::vector<fs::path> PendingPreviewWork;

    std::mutex DirMutex;

    std::shared_mutex PropertyMutex;
    Project::ProjectData Data;
    Config Cfg;
    std::vector<fs::path> LoadedPreviews;

    class CropperSignalRouter* Router;

    // We give the cropper a few updates before triggering done
    // so we don't ping-pong start<->done when we work fast
    static inline constexpr uint32_t UpdatesBeforeDoneTrigger{ 6 };

    QThread* CropThread;
    std::atomic_uint32_t CropDone{ 0 };

    QThread* PreviewThread;
    std::atomic_uint32_t PreviewDone{ 0 };

    std::atomic_bool Pause{ false };
    std::atomic_uint32_t ThreadsPaused{ 0 };

    std::atomic_bool Quit{ false };
    std::atomic_uint32_t ThreadsDone{ 0 };

    QTimer* CropTimer;
    QTimer* PreviewTimer;
};
