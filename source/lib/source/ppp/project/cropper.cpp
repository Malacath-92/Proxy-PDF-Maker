#include <ppp/project/cropper.hpp>

#include <ranges>

#include <QDebug>
#include <QThreadPool>
#include <QTimer>

#include <fmt/chrono.h>

#include <ppp/qt_util.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/cropper_work.hpp>

Cropper::Cropper(std::function<const cv::Mat*(std::string_view)> get_color_cube,
                 const Project& project)
    : m_Project{ project }
    , m_GetColorCube{ std::move(get_color_cube) }
    , m_ImageDB{ ImageDataBase::FromFile(project.m_Data.m_CropDir / ".image.db") }
{
    m_CropFinishedTimer.setSingleShot(true);
    m_CropFinishedTimer.setInterval(c_IdleTimeBeforeDoneTrigger);

    QObject::connect(&m_CropFinishedTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         m_TotalCropWorkToDo = 0;
                         m_TotalCropWorkDone = 0;

                         const auto crop_work_end_point{ std::chrono::high_resolution_clock::now() };
                         const auto crop_work_duration{ crop_work_end_point - m_CropWorkStartPoint };
                         LogInfo("Cropper finished...\nTotal Work Items: {}\nTotal Time Taken: {}",
                                 m_TotalCropWorkDone,
                                 std::chrono::duration_cast<std::chrono::seconds>(crop_work_duration));

                         CropWorkDone();
                     });
}
Cropper::~Cropper()
{
    for (auto& [_, work] : m_CropWork)
    {
        work->Cancel();
    }
    for (auto& [_, work] : m_PreviewWork)
    {
        work->Cancel();
    }

    while (m_AliveCropperWork.load(std::memory_order_acquire))
        /* spin */;

    m_ImageDB.Write();
}

void Cropper::Start()
{
    OnStart();

    m_State = State::Running;
}

void Cropper::ClearCropWork()
{
    OnClearCropWork();
}

void Cropper::ClearPreviewWork()
{
    OnClearPreviewWork();
}

void Cropper::CropDirChanged()
{
    m_ImageDB.Write();
    m_ImageDB.Read(m_Project.m_Data.m_CropDir / ".image.db");
}

void Cropper::CardAdded(const fs::path& card_name, bool needs_crop, bool needs_preview)
{
    PushWork(card_name, needs_crop, needs_preview);
}

void Cropper::CardRemoved(const fs::path& card_name)
{
    RemoveWork(card_name);

    if (m_State == State::Paused)
    {
        return;
    }

    if (fs::exists(m_Project.m_Data.m_CropDir / card_name))
    {
        fs::remove(m_Project.m_Data.m_CropDir / card_name);
        fs::remove(m_Project.m_Data.m_UncropDir / card_name);

        for (const auto& entry : fs::recursive_directory_iterator(m_Project.m_Data.m_CropDir))
        {
            if (entry.is_directory() && fs::exists(entry.path() / card_name))
            {
                fs::remove(entry.path() / card_name);
            }
        }
    }
}

void Cropper::CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name)
{
    RemoveWork(old_card_name);

    if (fs::exists(m_Project.m_Data.m_CropDir / old_card_name))
    {
        fs::rename(m_Project.m_Data.m_CropDir / old_card_name, m_Project.m_Data.m_CropDir / new_card_name);
    }

    for (const auto& entry : fs::recursive_directory_iterator(m_Project.m_Data.m_CropDir))
    {
        if (entry.is_directory() && fs::exists(entry.path() / old_card_name))
        {
            fs::rename(entry.path() / old_card_name, entry.path() / new_card_name);
        }
    }
}

void Cropper::CardModified(const fs::path& card_name)
{
    PushWork(card_name, true, true);
}

void Cropper::PauseWork()
{
    m_State = State::Paused;

    for (auto& [_, work] : m_CropWork)
    {
        work->Pause();
    }
    for (auto& [_, work] : m_PreviewWork)
    {
        work->Pause();
    }

    while (m_RunningCropperWork.load(std::memory_order_acquire))
        /* spin */;
}

void Cropper::RestartWork()
{
    for (auto& [_, work] : m_CropWork)
    {
        work->Unpause();
    }
    for (auto& [_, work] : m_PreviewWork)
    {
        work->Unpause();
    }

    m_State = State::Running;
}

void Cropper::PushWork(const fs::path& card_name, bool needs_crop, bool needs_preview)
{
    if (needs_crop)
    {
        if (!m_CropWork.contains(card_name))
        {
            auto* crop_work{
                new CropperCropWork{
                    m_AliveCropperWork,
                    m_RunningCropperWork,
                    card_name,
                    m_GetColorCube,
                    m_ImageDB,
                    m_Project.m_Data }
            };

            QObject::connect(this,
                             &Cropper::OnStart,
                             crop_work,
                             &CropperWork::Start);
            QObject::connect(this,
                             &Cropper::OnClearCropWork,
                             crop_work,
                             &CropperWork::Cancel);

            QObject::connect(crop_work,
                             &CropperPreviewWork::Finished,
                             this,
                             [this, card_name, crop_work]()
                             {
                                 if (m_CropWork.contains(card_name) &&
                                     m_CropWork[card_name] == crop_work)
                                 {
                                     m_CropWork.erase(card_name);
                                 }

                                 m_TotalCropWorkDone++;
                                 if (m_TotalCropWorkDone == m_TotalCropWorkToDo)
                                 {
                                     m_CropFinishedTimer.start();
                                 }
                                 else
                                 {
                                     const float progress{
                                         float(m_TotalCropWorkDone) / m_TotalCropWorkToDo,
                                     };
                                     CropProgress(progress);
                                 }
                             });

            if (m_TotalCropWorkToDo == 0)
            {
                m_CropWorkStartPoint = std::chrono::high_resolution_clock::now();
                CropWorkStart();
            }

            m_CropWork[card_name] = crop_work;
            m_TotalCropWorkToDo++;

            m_CropFinishedTimer.stop();

            if (m_State == State::Running)
            {
                crop_work->Start();
            }
        }
    }

    if (needs_preview)
    {
        if (!m_PreviewWork.contains(card_name))
        {
            auto* preview_work{
                new CropperPreviewWork{
                    m_AliveCropperWork,
                    card_name,
                    !m_Project.m_Data.m_Previews.contains(card_name),
                    m_ImageDB,
                    m_Project.m_Data }
            };

            QObject::connect(this,
                             &Cropper::OnStart,
                             preview_work,
                             &CropperWork::Start);
            QObject::connect(this,
                             &Cropper::OnClearPreviewWork,
                             preview_work,
                             &CropperWork::Cancel);

            QObject::connect(preview_work,
                             &CropperPreviewWork::PreviewUpdated,
                             this,
                             &Cropper::PreviewUpdated);
            QObject::connect(preview_work,
                             &CropperPreviewWork::Finished,
                             this,
                             [this, card_name, preview_work]()
                             {
                                 if (m_PreviewWork.contains(card_name) &&
                                     m_PreviewWork[card_name] == preview_work)
                                 {
                                     m_PreviewWork.erase(card_name);
                                 }
                             });

            m_PreviewWork[card_name] = preview_work;

            if (m_State == State::Running)
            {
                preview_work->Start();
            }
        }
    }
}

void Cropper::RemoveWork(const fs::path& card_name)
{
    if (m_CropWork.contains(card_name))
    {
        m_CropWork[card_name]->Cancel();
    }
    if (m_PreviewWork.contains(card_name))
    {
        m_PreviewWork[card_name]->Cancel();
    }

    m_CropWork.erase(card_name);
    m_PreviewWork.erase(card_name);
}
