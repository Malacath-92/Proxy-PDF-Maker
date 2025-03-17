#pragma once

#include <efsw/efsw.hpp>

#include <QObject>

#include <ppp/util.hpp>

class CardProvider : public QObject, public efsw::FileWatchListener
{
    Q_OBJECT;

  public:
    CardProvider(const fs::path& image_dir, const fs::path& crop_dir);

    void Start();

    void Refresh(const fs::path& image_dir, const fs::path& crop_dir);

    virtual void handleFileAction(efsw::WatchID watchid,
                                  const std::string& dir,
                                  const std::string& filename,
                                  efsw::Action action,
                                  std::string old_filename) override;

  signals:
    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

  private:
    fs::path ImageDir;
    fs::path CropDir;

    efsw::FileWatcher Watcher;
};
