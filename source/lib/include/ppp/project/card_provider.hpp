#pragma once

#include <efsw/efsw.hpp>

#include <QObject>

#include <ppp/util.hpp>

class Project;
struct ProjectData;

class CardProvider : public QObject, public efsw::FileWatchListener
{
    Q_OBJECT

  public:
    CardProvider(const Project& project);

    void Start();

    void NewProjectOpened(const ProjectData& old_project, const ProjectData& new_project);
    void ImageDirChanged(const fs::path& old_path, const fs::path& new_path);
    void CardSizeChanged();
    void BleedChanged();
    void ColorCubeChanged();
    void BasePreviewWidthChanged();
    void MaxDPIChanged();

    void ExternalCardAdded(const fs::path& absolute_image_path);
    void ExternalCardRemoved(const fs::path& absolute_image_path);

  private:
    virtual void handleFileAction(efsw::WatchID watchid,
                                  const std::string& dir,
                                  const std::string& filename,
                                  efsw::Action action,
                                  std::string old_filename) override;
    
    struct SWatch
    {
        fs::path m_Directory;
        std::vector<fs::path> m_Files;
    };

    void RegisterInitialWatches(const ProjectData& project);

    void RegisterExternalCard(const fs::path& absolute_image_path);
    void UnregisterExternalCard(const fs::path& absolute_image_path);

    SWatch* FindWatch(const fs::path& directory);
    void EraseWatch(const fs::path& directory);

    std::vector<fs::path> ListFiles() const;
    std::vector<fs::path> ListFiles(const SWatch& watch) const;
    template<class FunT>
    static void ListFiles(const SWatch& watch, FunT&& fun);

  signals:
    void CardAdded(const fs::path& card_name, bool needs_crop, bool needs_preview);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

  private:
    std::vector<SWatch> m_Watches;

    efsw::FileWatcher m_Watcher;
    std::vector<efsw::WatcherOption> m_WatcherOptions;

    bool m_Started{ false };
};
