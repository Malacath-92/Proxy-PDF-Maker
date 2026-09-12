#pragma once

#include <QByteArray>
#include <QObject>
#include <QRunnable>

#include <ppp/util.hpp>

class ZipWorkerBase
    : public QObject,
      public QRunnable
{
    Q_OBJECT

  public:
    enum class Conclusion
    {
        Pending,
        Failed,
        Success,
    };
    Conclusion GetConclusion() const;

    bool HasError() const;
    const std::string& GetError() const;

  signals:
    void Done();
    void Progress(float progress);

  protected:
    void Failed();
    void Failed(std::string error);
    void Succeeded();

  private:
    Conclusion m_Conclusion{ Conclusion::Pending };
    std::string m_Error;
};

class UnzipWorker : public ZipWorkerBase
{
    Q_OBJECT

  public:
    UnzipWorker(QByteArray archive_data,
                fs::path output_folder);

    virtual void run() override;

  private:
    QByteArray m_ArchiveData;
    fs::path m_OutputFolder;
};

class ZipWorker : public ZipWorkerBase
{
    Q_OBJECT

  public:
    ZipWorker(std::vector<std::pair<fs::path, fs::path>> files,
              fs::path output_file);

    virtual void run() override;

  private:
    std::vector<std::pair<fs::path, fs::path>> m_Files;
    fs::path m_OutputFile;
};
