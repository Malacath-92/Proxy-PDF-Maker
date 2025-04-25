#pragma once

#include <QByteArray>

#include <ppp/util.hpp>

struct ImageParameters
{
    PixelDensity DPI{ 0_dpi };
    Pixel Width{ 0_pix };
    Size CardSize{ 0_mm, 0_mm };
    Length FullBleedEdge{ 0_mm };
    bool WillWriteOutput{ true };
};

struct ImageDataBaseEntry
{
    QByteArray SourceHash;
    ImageParameters Params;
};

class ImageDataBase
{
  public:
    static ImageDataBase Read(const fs::path& path);
    void Write(const fs::path& path);

    // Tests if the mapping exists in the database and returns:
    //  - an empty hash if matches
    //  - the source hash if it mismatches
    // Note: Assumes source exists, if it doesn't an empty hash will be returned
    QByteArray TestEntry(const fs::path& destination, const fs::path& source, ImageParameters params) const;

    // Puts the given mapping into the database
    void PutEntry(const fs::path& destination, QByteArray source_hash, ImageParameters params);

  private:
    std::unordered_map<fs::path, ImageDataBaseEntry> DataBase;
};
