#include <ppp/project/card_provider.hpp>

#include <ppp/config.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

CardProvider::CardProvider(const Project& project)
    : ImageDir{ project.Data.ImageDir }
    , CropDir{ CFG.EnableUncrop ? project.Data.CropDir : "" }
    , OutputDir{ GetOutputDir(project.Data.CropDir, project.Data.BleedEdge, CFG.ColorCube) }
{
    Watcher.addWatch(ImageDir.string(), this, false);
    if (!CropDir.empty())
    {
        Watcher.addWatch(CropDir.string(), this, false);
    }
}

void CardProvider::Start()
{
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, true);
    }

    if (!Started)
    {
        Watcher.watch();
        Started = true;
    }
}

void CardProvider::NewProjectOpened(const Project& project)
{
    ImageDirChanged(project);
}
void CardProvider::ImageDirChanged(const Project& project)
{
    // Image folder, and thus crop folder has changed, we have to
    // remove all old files ...
    for (const fs::path& image : ListFiles())
    {
        CardRemoved(image);
    }

    Watcher.removeWatch((ImageDir / "").string());
    if (!CropDir.empty())
    {
        Watcher.removeWatch((CropDir / "").string());
    }

    ImageDir = project.Data.ImageDir;
    CropDir = project.Data.CropDir;
    OutputDir = GetOutputDir(project.Data.CropDir, project.Data.BleedEdge, CFG.ColorCube);

    // ... and add all new files ...
    Start();
}
void CardProvider::BleedChanged(const Project& project)
{
    OutputDir = GetOutputDir(project.Data.CropDir, project.Data.BleedEdge, CFG.ColorCube);

    // Generate new crops only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, false);
    }
}
void CardProvider::EnableUncropChanged(const Project& project)
{
    // CFG.EnableUncrop option has changed, so we need to add/remove cards
    // that are in the crop folder but not the images folder ...

    const auto images{
        ListImageFiles(ImageDir)
    };
    if (!CropDir.empty())
    {
        // CFG.EnableUncrop was enabled, is not anymore, so we remove the
        // difference ...
        const auto crop_images{
            ListImageFiles(CropDir)
        };
        for (const fs::path& image : crop_images)
        {
            if (!std::ranges::contains(images, image))
            {
                CardRemoved(image);
            }
        }

        // ... and remove the old watch ...
        CropDir.clear();
        Watcher.removeWatch((CropDir / "").string());
    }
    else
    {
        // CFG.EnableUncrop was not enabled, now it is, so we add the
        // difference ... ...
        const auto crop_images{
            ListImageFiles(project.Data.CropDir)
        };
        for (const fs::path& image : crop_images)
        {
            if (!std::ranges::contains(images, image))
            {
                CardAdded(image, true, true);
            }
        }

        // ... and add a new watch ...
        CropDir = project.Data.CropDir;
        Watcher.addWatch(CropDir.string(), this, false);
    }
}
void CardProvider::ColorCubeChanged(const Project& project)
{
    OutputDir = GetOutputDir(project.Data.CropDir, project.Data.BleedEdge, CFG.ColorCube);

    // Generate new crops only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, true, false);
    }
}
void CardProvider::BasePreviewWidthChanged(const Project& /*project*/)
{
    // Generate new previews only ...
    for (const fs::path& image : ListFiles())
    {
        CardAdded(image, false, true);
    }
}
void CardProvider::MaxDPIChanged(const Project& /*project*/)
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
    switch (action)
    {
    case efsw::Action::Add:
        if (filepath.has_extension())
        {
            CardAdded(filepath, true, true);
        }
        break;
    case efsw::Action::Delete:
        if (filepath.has_extension())
        {
            CardRemoved(filepath);
        }
        break;
    case efsw::Action::Modified:
        if (filepath.has_extension())
        {
            CardModified(filepath);
        }
        break;
    case efsw::Action::Moved:
        if (filepath.has_extension())
        {
            CardRenamed(old_filename, filepath);
        }
        break;
    }
}

std::vector<fs::path> CardProvider::ListFiles()
{
    return !CropDir.empty()
               ? ListImageFiles(ImageDir, CropDir)
               : ListImageFiles(ImageDir);
}
