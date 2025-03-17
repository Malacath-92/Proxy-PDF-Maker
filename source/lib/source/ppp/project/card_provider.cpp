#include <ppp/project/card_provider.hpp>

#include <ppp/project/image_ops.hpp>

CardProvider::CardProvider(const fs::path& image_dir, const fs::path& crop_dir)
{
    Refresh(image_dir, crop_dir);
}

void CardProvider::Start()
{
    const auto images{
        CFG.EnableUncrop
            ? ListImageFiles(ImageDir, CropDir)
            : ListImageFiles(ImageDir)
    };
    for (const fs::path& image : images)
    {
        CardAdded(image);
    }

    Watcher.watch();
}

void CardProvider::Refresh(const fs::path& image_dir, const fs::path& crop_dir)
{
    if (!ImageDir.empty())
    {
        Watcher.removeWatch((ImageDir / "").string());
    }
    if (!CropDir.empty())
    {
        Watcher.removeWatch((CropDir / "").string());
    }

    {
        const auto images{
            CFG.EnableUncrop
                ? ListImageFiles(ImageDir, CropDir)
                : ListImageFiles(ImageDir)
        };
        for (const fs::path& image : images)
        {
            CardRemoved(image);
        }
    }

    ImageDir = image_dir;
    CropDir = crop_dir;

    if (CFG.EnableUncrop)
    {
        Watcher.addWatch(ImageDir.string(), this, false);
        Watcher.addWatch(CropDir.string(), this, false);
    }
    else
    {
        Watcher.addWatch(ImageDir.string(), this, false);
    }
}

void CardProvider::handleFileAction(efsw::WatchID /*watchid*/,
                                    const std::string& /*dir*/,
                                    const std::string& filename,
                                    efsw::Action action,
                                    std::string old_filename)
{
    switch (action)
    {
    case efsw::Action::Add:
        CardAdded(filename);
        break;
    case efsw::Action::Delete:
        CardRemoved(filename);
        break;
    case efsw::Action::Modified:
        CardModified(filename);
        break;
    case efsw::Action::Moved:
        CardRenamed(old_filename, filename);
        break;
    }
}
