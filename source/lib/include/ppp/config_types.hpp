#pragma once

enum class CardOrder
{
    Alphabetical,
    Backside,
    LastModified,
    LastAdded,
};

enum class CardOrderDirection
{
    Ascending,
    Descending,
};

enum class PdfBackend
{
    PoDoFo,
    Png,
};

enum class ImageCompression
{
    Lossless,
    Lossy,
    AsIs,
};

enum class PageOrientation
{
    Portrait,
    Landscape
};
