#pragma once

#include <functional>

#include <QObject>

#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

class CropperSignalRouter : public QObject
{
    Q_OBJECT

  public:
    template<class FunT>
    void SetWork(FunT&& work)
    {
        Work = std::forward<FunT>(work);
    }

  signals:
    void CropWorkStart();
    void CropWorkDone();

    void PreviewWorkStart();
    void PreviewWorkDone();

    void CropProgress(float progress);
    void PreviewUpdated(const fs::path& card_name, const ImagePreview& preview);

  public slots:
    void DoWork()
    {
        Work();
    }

  private:
    std::function<void()> Work;
};
