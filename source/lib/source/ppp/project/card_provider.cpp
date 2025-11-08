#include <ppp/project/card_provider.hpp>

#include <ranges>

#include <ppp/config.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#ifdef _WIN32
#include "Windows.h"
#endif

CardProvider::CardProvider(const Project& project)
    : m_WatcherOptions{
#ifdef _WIN32
        efsw::WatcherOption{
            efsw::Option::WinNotifyFilter,
            FILE_NOTIFY_CHANGE_FILE_NAME,
        },
#endif
    }
{
    RegisterInitialWatches(project.m_Data);
}

void CardProvider::Start()
{
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, true);
    }

    if (!m_Started)
    {
        m_Watcher.watch();
        m_Started = true;
    }
}

void CardProvider::NewProjectOpened(const ProjectData& /*old_project*/, const ProjectData& new_project)
{
    // Remove all old files ...
    for (const fs::path& image : ListFiles())
    {
        CardRemoved(image);
    }

    // ... unregister all watches ...
    for (const SWatch& watch : m_Watches)
    {
        m_Watcher.removeWatch((watch.m_Directory / "").string());
    }
    m_Watches.clear();

    // ... register new watches ...
    RegisterInitialWatches(new_project);

    // ... and add all new files.
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, true);
    }
}
void CardProvider::ImageDirChanged(const fs::path& old_path, const fs::path& new_path)
{
    SWatch* old_watch{ FindWatch(old_path) };
    if (old_watch == nullptr)
    {
        return;
    }

    // Remove all old files ...
    for (const fs::path& image : ListFiles(*old_watch))
    {
        // ... but don't remove explicitly watched images ...
        if (!std::ranges::contains(old_watch->m_Files, image))
        {
            CardRemoved(image);
        }
    }

    {
        // ... stop watching all files in the old watch ...
        if (old_watch->m_Files.size() == 1)
        {
            // ... by removing the watch ...
            EraseWatch(old_path);
            m_Watcher.removeWatch((old_path / "").string());
        }
        else
        {
            // ... or narrowing down the explicitly watched files  ...
            auto it{ std::ranges::find(old_watch->m_Files, "*") };
            if (it != old_watch->m_Files.end())
            {
                old_watch->m_Files.erase(it);
            }
        }

        // ... watch the new folder ...
        SWatch* new_watch{ FindWatch(new_path) };
        if (new_watch == nullptr)
        {
            // ... by creating a new watch ...
            m_Watches.push_back({ new_path,
                                  { "*" } });
            m_Watcher.addWatch(new_path.string(),
                               this,
                               false,
                               m_WatcherOptions);
        }
        else
        {
            // ... or by expanding the existing watch to all files ...
            new_watch->m_Files.push_back("*");
        }
    }

    // ... add all new files, including external images,
    //     as those require output in the image folder.
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, true);
    }
}
void CardProvider::CardSizeChanged()
{
    // Generate new crops and previews ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, true);
    }
}
void CardProvider::BleedChanged()
{
    // Generate new crops only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, false);
    }
}
void CardProvider::ColorCubeChanged()
{
    // Generate new crops only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, false);
    }
}
void CardProvider::BasePreviewWidthChanged()
{
    // Generate new previews only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, false, true);
    }
}
void CardProvider::MaxDPIChanged()
{
    // Generate new crops only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, false);
    }
}

void CardProvider::ExternalCardAdded(const fs::path& absolute_image_path)
{
    RegisterExternalCard(absolute_image_path);

    if (m_Started)
    {
        CardAdded(absolute_image_path.filename(), true, true);
    }
}
void CardProvider::ExternalCardRemoved(const fs::path& absolute_image_path)
{
    UnregisterExternalCard(absolute_image_path);

    if (m_Started)
    {
        CardRemoved(absolute_image_path.filename());
    }
}

void CardProvider::handleFileAction(efsw::WatchID /*watchid*/,
                                    const std::string& dir,
                                    const std::string& filename,
                                    efsw::Action action,
                                    std::string old_filename)
{
    const fs::path filepath{ filename };

    // A sub-folder has changed, ignore this
    if (!filepath.has_extension())
    {
        return;
    }

    // A non-image has changed, ignore this
    if (!std::ranges::contains(g_ValidImageExtensions, filepath.extension()))
    {
        return;
    }

    auto* watch{ FindWatch(dir.substr(0, dir.size() - 1)) };
    if (watch == nullptr)
    {
        return;
    }

    const fs::path old_filepath{ old_filename };

    if (!std::ranges::contains(watch->m_Files, "*") &&
        !std::ranges::contains(watch->m_Files, filepath) &&
        !std::ranges::contains(watch->m_Files, old_filepath))
    {
        return;
    }

    switch (action)
    {
    case efsw::Action::Add:
        CardAdded(filepath, true, true);
        break;
    case efsw::Action::Delete:
        CardRemoved(filepath);
        break;
    case efsw::Action::Modified:
        CardModified(filepath);
        break;
    case efsw::Action::Moved:
        CardRenamed(old_filename, filepath);

        {
            auto it{ std::ranges::find(watch->m_Files, old_filepath) };
            if (it != watch->m_Files.end())
            {
                *it = filepath;
            }
        }
        break;
    }
}

void CardProvider::RegisterInitialWatches(const ProjectData& project)
{
    m_Watches.push_back(SWatch{ project.m_ImageDir,
                                { "*" } });
    m_Watcher.addWatch(project.m_ImageDir.string(),
                       this,
                       false,
                       m_WatcherOptions);

    auto external_pathes{
        project.m_Cards |
        std::views::filter([](const auto& card)
                           { return card.m_ExternalPath.has_value(); }) |
        std::views::transform([](const auto& card)
                              { return std::cref(card.m_ExternalPath.value()); })
    };
    for (const auto& external_path : external_pathes)
    {
        RegisterExternalCard(external_path);
    }
}

void CardProvider::RegisterExternalCard(const fs::path& absolute_image_path)
{
    const auto parent_path{ absolute_image_path.parent_path() };
    auto* watch{ FindWatch(parent_path) };
    if (watch == nullptr)
    {
        m_Watches.push_back(SWatch{ parent_path, {} });
        m_Watcher.addWatch(parent_path.string(),
                           this,
                           false,
                           m_WatcherOptions);
        watch = &m_Watches.back();
    }

    watch->m_Files.push_back(absolute_image_path.filename());
}
void CardProvider::UnregisterExternalCard(const fs::path& absolute_image_path)
{
    const auto parent_path{ absolute_image_path.parent_path() };
    auto* watch{ FindWatch(parent_path) };
    if (watch == nullptr)
    {
        m_Watches.push_back(SWatch{ parent_path, {} });
        watch = &m_Watches.back();
    }

    {
        auto it{ std::ranges::find(watch->m_Files, absolute_image_path.filename()) };
        if (it != watch->m_Files.end())
        {
            watch->m_Files.erase(it);
        }
    }

    if (watch->m_Files.empty())
    {
        m_Watcher.removeWatch((parent_path / "").string());
        EraseWatch(parent_path);
    }
}

CardProvider::SWatch* CardProvider::FindWatch(const fs::path& directory)
{
    auto it{ std::ranges::find(m_Watches, directory, &SWatch::m_Directory) };
    if (it != m_Watches.end())
    {
        return &*it;
    }
    return nullptr;
}
void CardProvider::EraseWatch(const fs::path& directory)
{
    auto it{ std::ranges::find(m_Watches, directory, &SWatch::m_Directory) };
    if (it != m_Watches.end())
    {
        m_Watches.erase(it);
    }
}

std::vector<fs::path> CardProvider::ListFiles() const
{
    std::vector<fs::path> files{};
    auto push_file{
        [&files](const auto& file)
        {
            if (!std::ranges::contains(files, file.filename()))
            {
                files.push_back(file.filename());
            }
        }
    };

    for (const auto& watch : m_Watches)
    {
        ListFiles(watch, push_file);
    }

    return files;
}
std::vector<fs::path> CardProvider::ListFiles(const SWatch& watch) const
{
    std::vector<fs::path> files{};
    auto push_file{
        [&files](const auto& file)
        {
            if (!std::ranges::contains(files, file.filename()))
            {
                files.push_back(file.filename());
            }
        }
    };

    ListFiles(watch, push_file);

    return files;
}
template<class FunT>
void CardProvider::ListFiles(const SWatch& watch, FunT&& fun)
{
    const bool all_files{ std::ranges::contains(watch.m_Files, "*") };
    ForEachFile(
        watch.m_Directory, [&, fun = std::forward<FunT>(fun)](const auto& file)
        {
                if (all_files || std::ranges::contains(watch.m_Files, file.filename()))
                {
                    fun(file.filename());
                } },
        g_ValidImageExtensions);
}
