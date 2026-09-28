#pragma once

#include <QString>

struct YuGiOhDownloaderSettings
{
    static YuGiOhDownloaderSettings Read();
    void Write();

    QString m_UpscaleModel{ "None" };

    bool m_AdjustSettings{ true };
    bool m_ClearImages{ true };
};
