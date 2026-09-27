#pragma once

#include <QString>

struct MtgDownloaderSettings
{
    static MtgDownloaderSettings Read();
    void Write();

    QString m_UpscaleModel{ "None" };

    bool m_AdjustSettings{ true };
    bool m_DownloadBacksides{ true };
    bool m_ArtCrops{ false };
    bool m_ClearImages{ true };
    bool m_FillCorners{ true };
};
