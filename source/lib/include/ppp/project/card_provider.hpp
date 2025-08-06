#pragma once

#include <efsw/efsw.hpp>

#include <QObject>

#include <ppp/util.hpp>

class Project;

class CardProvider : public QObject, public efsw::FileWatchListener
{
    Q_OBJECT

  public:
    CardProvider(const Project& project);

    void Start();

    void NewProjectOpened();
    void ImageDirChanged();
    void CardSizeChanged();
    void BleedChanged();
    void ColorCubeChanged();
    void BasePreviewWidthChanged();
    void MaxDPIChanged();

  private:
    virtual void handleFileAction(efsw::WatchID watchid,
                                  const std::string& dir,
                                  const std::string& filename,
                                  efsw::Action action,
                                  std::string old_filename) override;

    std::vector<fs::path> ListFiles();

  signals:
    void CardAdded(const fs::path& card_name, bool needs_crop, bool needs_preview);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

  private:
    const Project& m_Project;

    fs::path m_ImageDir;

    efsw::FileWatcher m_Watcher;

    bool m_Started{ false };
};
