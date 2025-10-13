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

class CropperWork;

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
    void CropWorkDone(std::chrono::seconds time,
                      uint32_t work_done,
                      uint32_t work_skipped);

    void PreviewWorkStart();
    void PreviewWorkDone();

    void CropProgress(float progress);
    void PreviewUpdated(const fs::path& card_name,
                        const ImagePreview& preview,
                        Image::Rotation rotation);

    void OnStart();

    void OnClearCropWork();
    void OnClearPreviewWork();

  public slots:
    void CropDirChanged();

    void CardAdded(const fs::path& card_name, bool needs_crop, bool needs_preview);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

    void PauseWork();
    void RestartWork();

  private:
    void PushWork(const fs::path& card_name, bool needs_crop, bool needs_preview);
    void RemoveWork(const fs::path& card_name);

    const Project& m_Project;

    enum class State
    {
        Waiting,
        Running,
        Paused,
    };
    State m_State{ State::Waiting };

    std::function<const cv::Mat*(std::string_view)> m_GetColorCube;

    std::unordered_map<fs::path, CropperWork*> m_CropWork;
    std::unordered_map<fs::path, CropperWork*> m_PreviewWork;

    uint32_t m_TotalCropWorkToDo{ 0 };
    uint32_t m_TotalCropWorkDone{ 0 };
    uint32_t m_TotalCropWorkSkipped{ 0 };

    std::atomic_uint32_t m_AliveCropperWork{};
    std::atomic_uint32_t m_RunningCropperWork{};

    ImageDataBase m_ImageDB;

    using time_point = decltype(std::chrono::high_resolution_clock::now());
    time_point m_CropWorkStartPoint;

    // We give the cropper a a bit of time before triggering done
    // so we don't ping-pong start<->done when we work fast
    QTimer m_CropFinishedTimer;
    static inline constexpr std::chrono::milliseconds c_IdleTimeBeforeDoneTrigger{ 250 };
};
