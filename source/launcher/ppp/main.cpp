#include <QLibrary>

class AppLibrary
{
  public:
    ~AppLibrary();

    [[nodiscard]] bool Load();
    [[nodiscard]] bool Initialize(int argc, char** argv);
    [[nodiscard]] bool Execute();
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

int main(int argc, char** argv)
{
    AppLibrary app_lib{};
    if (!app_lib.Load())
    {
        return 1;
    }

    if (!app_lib.Initialize(argc, argv))
    {
        return 2;
    }

    if (!app_lib.Execute())
    {
        return 3;
    }

    if (!app_lib.Shutdown())
    {
        return 4;
    }

    return app_lib.ReturnCode();
}

bool AppLibrary::Load()
{
    return m_Lib.load();
}

template<class FunT>
struct Call
{
};

template<class RetT, class... ArgsT>
struct Call<RetT(ArgsT...)>
{
    static RetT operator()(QFunctionPointer fun,
                           ArgsT... args)
    {
        return reinterpret_cast<RetT (*)(ArgsT...)>(fun)(std::forward<ArgsT>(args)...);
    }
};

AppLibrary::~AppLibrary()
{
    if (m_Lib.isLoaded())
    {
        (void)Shutdown();
    }
}

bool AppLibrary::Initialize(int argc, char** argv)
{
    auto init_fun{ m_Lib.resolve("Initialize") };
    if (init_fun == nullptr)
    {
        return false;
    }

    m_UserData = Call<void*(int, char**)>{}(init_fun,
                                            argc,
                                            argv);

    return m_UserData != nullptr;
}

bool AppLibrary::Execute()
{
    auto exec_fun{ m_Lib.resolve("Execute") };
    if (exec_fun == nullptr)
    {
        return false;
    }

    m_ReturnCode = Call<int(void*)>{}(exec_fun,
                                      m_UserData);

    return true;
}

bool AppLibrary::Shutdown()
{
    if (!m_Lib.isLoaded())
    {
        return false;
    }

    auto shutdown_fun{ m_Lib.resolve("Shutdown") };
    if (shutdown_fun == nullptr)
    {
        return false;
    }

    Call<int(void*)>{}(shutdown_fun,
                       m_UserData);

    return m_Lib.unload();
}
