#include <ppp/project/card_provider.hpp>

#include <ppp/config.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

CardProvider::CardProvider(const Project& project)
    : m_Project{ project }
    , m_ImageDir{ project.Data.ImageDir }
    , m_CropDir{ CFG.EnableUncrop ? project.Data.CropDir : "" }
    , m_OutputDir{ GetOutputDir(project.Data.CropDir, project.Data.BleedEdge, CFG.ColorCube) }
{
    m_Watcher.addWatch(m_ImageDir.string(), this, false);
    if (!m_CropDir.empty())
    {
        m_Watcher.addWatch(m_CropDir.string(), this, false);
    }
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
    if (!m_CropDir.empty())
    {
        m_Watcher.removeWatch((m_CropDir / "").string());
    }

    m_ImageDir = m_Project.Data.ImageDir;
    m_CropDir = m_Project.Data.CropDir;
    m_OutputDir = GetOutputDir(m_Project.Data.CropDir, m_Project.Data.BleedEdge, CFG.ColorCube);

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
    m_OutputDir = GetOutputDir(m_Project.Data.CropDir, m_Project.Data.BleedEdge, CFG.ColorCube);

    // Generate new crops only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, false);
    }
}
void CardProvider::EnableUncropChanged()
{
    // CFG.EnableUncrop option has changed, so we need to add/remove cards
    // that are in the crop folder but not the images folder ...

    const auto images{
        ListImageFiles(m_ImageDir)
    };
    if (!m_CropDir.empty())
    {
        // CFG.EnableUncrop was enabled, is not anymore, so we remove the
        // difference ...
        const auto crop_images{
            ListImageFiles(m_CropDir)
        };
        for (const fs::path& image : crop_images)
        {
            if (!std::ranges::contains(images, image))
            {
                CardRemoved(image);
            }
        }

        // ... and remove the old watch ...
        m_CropDir.clear();
        m_Watcher.removeWatch((m_CropDir / "").string());
    }
    else
    {
        // CFG.EnableUncrop was not enabled, now it is, so we add the
        // difference ... ...
        const auto crop_images{
            ListImageFiles(m_Project.Data.CropDir)
        };
        for (const fs::path& image : crop_images)
        {
            if (!std::ranges::contains(images, image))
            {
                CardAdded(image, true, true);
            }
        }

        // ... and add a new watch ...
        m_CropDir = m_Project.Data.CropDir;
        m_Watcher.addWatch(m_CropDir.string(), this, false);
    }
}
void CardProvider::ColorCubeChanged()
{
    m_OutputDir = GetOutputDir(m_Project.Data.CropDir, m_Project.Data.BleedEdge, CFG.ColorCube);

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
    return !m_CropDir.empty()
               ? ListImageFiles(m_ImageDir, m_CropDir)
               : ListImageFiles(m_ImageDir);
}
