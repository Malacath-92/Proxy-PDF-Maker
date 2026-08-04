#pragma once

#include <QObject>

#include <ppp/config.hpp>
#include <ppp/util.hpp>

class Project;

class ActionsViewModel : public QObject
{
    Q_OBJECT

    friend class ActionsWidget;

  public:
    ActionsViewModel(Project& project,
                     const Config& config);

  signals:
    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

    void RenderBackendChanged(PdfBackend backend);

  private slots:
    void RenderDocument() const;
    void SetImagesFolder(fs::path new_image_dir);
    void OpenImagesFolder() const;

  private:
    Project& m_Project;
    const Config& m_Cfg;
};
