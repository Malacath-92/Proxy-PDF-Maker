#pragma once

#include <QObject>

#include <ppp/config.hpp>
#include <ppp/util.hpp>

class Project;

class ActionsViewModel : public QObject
{
    Q_OBJECT

  public:
    ActionsViewModel(Project& project,
                     const Config& config);

    void EmitDefaults();

    bool VerifyProject() const;

  signals:
    // forward

    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

    void PdfBackendChanged(PdfBackend backend);

  public slots:
    void RenderDocument() const;
    fs::path GetImageFolderBase() const;
    void SetImagesFolder(fs::path new_image_dir);
    void OpenImagesFolder() const;

  private:
    Project& m_Project;
    const Config& m_Cfg;
};
