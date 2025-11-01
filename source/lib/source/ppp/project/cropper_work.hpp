#pragma once

#include <atomic>
#include <functional>

#include <QObject>
#include <QRunnable>

#include <ppp/config.hpp>
#include <ppp/project/project.hpp>

class ImageDataBase;

class CropperWork : public QObject, public QRunnable
{
    Q_OBJECT

  public:
    CropperWork(std::atomic_uint32_t& alive_cropper_work);
    ~CropperWork();

    void Start();
    void Restart();
    void Cancel();

    int Priority() const;

    enum Conclusion
    {
        Success,
        Failure,
        Skipped,
        Cancelled,
        RestartRequested,
    };

  public slots:
    void Pause();
    void Unpause();

  signals:
    void Finished(Conclusion conclusion) const;

    void OnUnpaused(QPrivateSignal) const;

  protected:
    bool EnterRun();

    std::atomic_uint32_t& m_AliveCropperWork;

    int m_Priorty{ 0 };

    inline static constexpr uint32_t c_MaxRetries{ 5 };
    std::atomic_uint32_t m_Retries{ 0 };

  private:
    enum class State
    {
        Waiting,
        Running,
        Paused,
        Cancelled,
    };
    std::atomic<State> m_State{ State::Waiting };
    std::atomic_bool m_CancelRequested{ false };
};

class CropperCropWork : public CropperWork
{
    Q_OBJECT

  public:
    CropperCropWork(
        std::atomic_uint32_t& alive_cropper_work,
        std::atomic_uint32_t& running_crop_work,
        fs::path card_name,
        std::function<const cv::Mat*(std::string_view)> get_color_cube,
        ImageDataBase& image_db,
        const Project& project);

    virtual void run() override;

  private:
    std::atomic_uint32_t& m_RunningCropperWork;

    fs::path m_CardName;
    Image::Rotation m_Rotation;
    BleedType m_BleedType;
    BadAspectRatioHandling m_BadAspectRatioHandling;

    std::function<const cv::Mat*(std::string_view)> m_GetColorCube;

    ImageDataBase& m_ImageDB;

    Project::ProjectData m_Data;
    Config m_Cfg;
};

class CropperPreviewWork : public CropperWork
{
    Q_OBJECT

  public:
    CropperPreviewWork(
        std::atomic_uint32_t& alive_cropper_work,
        fs::path card_name,
        bool force,
        ImageDataBase& image_db,
        const Project& project);

    virtual void run() override;

  signals:
    void PreviewUpdated(const fs::path& card_name,
                        const ImagePreview& preview,
                        Image::Rotation rotation);

  private:
    fs::path m_CardName;
    Image::Rotation m_Rotation;
    BleedType m_BleedType;
    BadAspectRatioHandling m_BadAspectRatioHandling;
    bool m_Force;

    ImageDataBase& m_ImageDB;

    Project::ProjectData m_Data;
    Config m_Cfg;
};
