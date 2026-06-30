#include <QLibrary>

class AppLibrary
{
  public:
    ~AppLibrary();

    [[nodiscard]] bool Load();
    [[nodiscard]] bool Initialize(int argc, char** argv);
    [[nodiscard]] bool Execute();
    [[nodiscard]] bool RebootRequested();
    [[nodiscard]] bool Shutdown();

    int ReturnCode() const
    {
        return m_ReturnCode;
    }

  private:
    QLibrary m_Lib{ "proxy_pdf" };
    void* m_UserData{ nullptr };
    int m_ReturnCode{ -1 };
};
