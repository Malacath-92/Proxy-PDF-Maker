# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.9.2] - 2025-22-04

### Added
- Another render option which is labeled "Alignment Test" that prints a small two-page pdf that you can print to test alignment

## [0.9.1] - 2025-22-04

### Fixed
- Cropper will no longer rerun on every startup

## [0.9.0] - 2025-20-04

### Added
- Experimental option for custom card sizes
- Option to export exact guides to svg

### Changed
- Cutting guides now don't draw into the card post cutting anymore

## [0.8.1] - 2025-13-04

### Fixed
- The app will no longer crash when trying to write a pdf with PoDoFo if that file is already opened in another app
- Cards will no longer reset all their data during startup

## [0.8.0] - 2025-13-04

### Added
- Another render backend, PoDoFo, which can be used to render on top of an existing pdf
- Option for changing margins, default values are the same as previous margins
- Widget for showing some info about physical sizes of the prints

### Changed
- Bumped OpenCV dependency to version 4.11.0

## [0.7.1] - 2025-08-04

### Fixed
- Png files written by the app now have the correct CRC in the pHYs chunk, some programs didn't like the error, most didn't care, so this may not affect you

## [0.7.0] - 2025-07-04

### Changed
- Improved reliability of cropper
- Crop work now happens asynchronously, blocking of UI happens only when rendering now
- Moved preview cache to crop folder
- Jpg files written to the output folder now contain DPI data

### Fixed
- Color cube option will now save and load correctly

## [0.6.0] - 2025-31-03

### Added
- Option to output pdf files with jpg images embedded, use `PDF.Backend.Image.Format=Jpg` in config.ini
    - Determine quality with `PDF.Backend.Jpg.Quality=N`, where `N` is a whole number between `1` and `100`

## [0.5.6] - 2025-29-03

### Fixed
- Dummy cards will no longer accidentally show up in prints, potentially causing a crash
- Paper size "Fit" will now reliably print the right card layout

## [0.5.5] - 2025-29-03

### Fixed
- The card layout widget, shown when paper size is set to "Fit", should now stay visible

## [0.5.4] - 2025-29-03

### Fixed
- Ignore the png restriction for the minimal cropper, otherwise it'd be broken right now

## [0.5.3] - 2025-28-03

### Fixed
- Only work with pngs, will resave any images that don't fit this
- Correctly hide dummy widgets

## [0.5.2] - 2025-28-03

### Fixed
- All images written by the program now contain DPI information
- The printout notifying of excessive DPI has been fixed to not mix up file name and DPI

## [0.5.1] - 2025-28-03

### Fixed
- Cards now display correctly after changing column count of card are

## [0.5.0] - 2025-26-03

### Added
- A `Fit` option for paper size, to allow for generating perfect sized output.
- Include `pHYs` chunk in generated pngs, which contains DPI information.

### Fixed
- Implemented cutting guides for png backend

## [0.4.0] - 2025-25-03

### Added
- Hidden png output option, to use set `PDF.Backend=Png` in config.ini
    - Use `PDF.Backend.Png.Compression=N` to control compression level, where `N` is a whole number between `0` and `9`. Higher numbers are significantly slower.

### Changed
- Sort cards in alphanumeric order as good as possible. This only fails when rendering with oversized cards.

## [0.3.1] - 2025-09-03

### Fixed
- Card grid will correctly refresh again when enabling/disabling backside options.

## [0.3.0] - 2025-08-03

### Added
- Option for guides on backside.

### Changed
- Turn options sidebar into a scroll view to allow smaller windows.
- Use UTF-8 locale on Windows machines.
- Slightly reduced size of image cache.
- Improved visuals of cutting guides in preview.

### Fixed
- Correctly populate card grid when cropper has not run yet.
- Keep info popup centered even during startup.
- The card grid will no longer look glitchy when reducing the amount of columns.

## [0.2.0] - 2025-28-02

### Added
- Automatic minimal cropping work now happens when rendering the document. For the full functionality of the program you will still need to run the full cropper.

### Fixed
- Bleed edge spin box will not reset anymore when the side panel refreshes.

## [0.1.0] - 2025-23-02

### Added
- Corner weight option that allows for moving the cutting guides.
- Support for oversized cards in the preview.

### Changed
- Reduced size of default backside image.
- Hide all backside options when backsides are disabled.
- Improved cutting guides accuracy in preview.

### Fixed
- Cancelling color picker will not set color to black.
- Rendering lots of pages will not cause a crash anymore.
- Non-filled pages will be at the end of the document now instead of the start.
- The App is now correctly named "Proxy PDF Maker" so settings won't clash with the old Python app.
- Guides colors in the preview will display correctly now.

## [0.0.0] - 2025-16-02

### Added
- Initial release, with same features as [print-proxy-prep](https://github.com/preshtildeath/print-proxy-prep).
- Configurable preview resolution.
- Multiple themes, including possibility to add more themes.
- Cutting guides and backside offset shown in preview.
- Configurable page sizes.
- Possibility to load color cubes dynamically.
- F1 window with link to Issues page on GitHub.
