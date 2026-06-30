#include <ppp/lib.hpp>

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

bool AppLibrary::Load()
{
    return m_Lib.load();
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

bool AppLibrary::RebootRequested()
{
    auto want_reboot_fun{ m_Lib.resolve("RebootRequested") };
    if (want_reboot_fun == nullptr)
    {
        return false;
    }

    return Call<bool(void*)>{}(want_reboot_fun,
                               m_UserData);
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
