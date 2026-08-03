#pragma once

#include <QObject>

#include <ppp/util.hpp>

class Project;

class ActionsViewModel : public QObject
{
    Q_OBJECT

    friend class ActionsWidget;

  public:
    ActionsViewModel(Project& project);

  signals:
    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

    void RenderBackendChanged();

  private slots:
    void RenderDocument() const;
    void SetImagesFolder(fs::path new_image_dir);
    void OpenImagesFolder() const;

  private:
    Project& m_Project;
};
