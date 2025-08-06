#include <ppp/project/card_provider.hpp>

#include <ppp/config.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

CardProvider::CardProvider(const Project& project)
    : m_Project{ project }
    , m_ImageDir{ project.m_Data.m_ImageDir }
{
    m_Watcher.addWatch(m_ImageDir.string(), this, false);
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

void CardProvider::NewProjectOpened()
{
    ImageDirChanged();
}
void CardProvider::ImageDirChanged()
{
    // Image folder, and thus crop folder has changed, we have to
    // remove all old files ...
    for (const fs::path& image : ListFiles())
    {
        CardRemoved(image);
    }

    m_Watcher.removeWatch((m_ImageDir / "").string());

    m_ImageDir = m_Project.m_Data.m_ImageDir;

    m_Watcher.addWatch(m_ImageDir.string(), this, false);

    // ... and add all new files ...
    Start();
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

void CardProvider::handleFileAction(efsw::WatchID /*watchid*/,
                                    const std::string& /*dir*/,
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
        break;
    }
}

std::vector<fs::path> CardProvider::ListFiles()
{
    return ListImageFiles(m_ImageDir);
}
