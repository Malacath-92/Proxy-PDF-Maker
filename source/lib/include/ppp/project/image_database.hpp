#pragma once

#include <QByteArray>

#include <ppp/util.hpp>

struct ImageParameters
{
    PixelDensity m_DPI{ 0_dpi };
    Pixel m_Width{ 0_pix };
    Size m_CardSize{ 0_mm, 0_mm };
    Length m_FullBleedEdge{ 0_mm };
    bool m_WillWriteOutput{ true };
};

struct ImageDataBaseEntry
{
    QByteArray m_SourceHash;
    ImageParameters m_Params;
};

class ImageDataBase
{
  public:
    static ImageDataBase Read(const fs::path& path);
    void Write(const fs::path& path);

    // Checks if the given file is part of the database at all, indicating that
    // we previously touched this file
    bool FindEntry(const fs::path& destination) const;

    // Tests if the mapping exists in the database and returns:
    //  - an empty hash if matches
    //  - the source hash if it mismatches
    // Note: Assumes source exists, if it doesn't an empty hash will be returned
    QByteArray TestEntry(const fs::path& destination, const fs::path& source, ImageParameters params) const;

    // Puts the given mapping into the database
    void PutEntry(const fs::path& destination, QByteArray source_hash, ImageParameters params);

  private:
    std::unordered_map<fs::path, ImageDataBaseEntry> m_DataBase;
};
