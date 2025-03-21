# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
