# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.1] - 2025-18-11

### Changed
- The default units are now `mm`.
- The label for restoring card order was made more general to make sense with user settings.
- Selected color cube will now be applied to previews as well.

### Fixed
- Some settings will no longer get messed up when changing units.
- The backside of a card will now correctly update when resetting it to default.
- A potential bug in the pdf generation that could result in corrupt pdf files is now gone.

## [1.0.0] - 2025-16-11

### Added
- When right-clicking on cards in the card grid or in the preview a new context menu will show up that gives a number of options for changing how images are interpreted:
    - _Remove External Card_: Removes the card from the project. **Only visible if the card is an external card.**
    - _Reset Backside_: Resets the backside for this card back to the default. **Only visible if the card has a non-default backside and backsides are enabled.**
    - Bleed Options:
        - _Infer Input Bleed_: Default setting. The app will try to determine whether the image has a bleed edge or not.
        - _Assume Full Bleed_: The image has a full bleed edge.
        - _Assume No Bleed_: The image does not have any bleed edge.
    - Aspect Ratio Options: **Only visible if the image has an unexpected aspect ratio**
        - _Reset Aspect Ratio_: Reset the aspect ratio to the images original aspect ratio.
        - _Fix Aspect Ratio: Expand_: Expands the image in one dimension to make it have the expected aspect ratio.
        - _Fix Aspect Ratio: Stretch_: Stretches the image in one dimension to make it have the expected aspect ratio.
    - _Rotate Left_: Rotates the image by 90 degrees counter-clockwise.
    - _Rotate Right_: Rotates the image by 90 degrees clockwise.
- It is now possible to drag-and-drop images into the app, these are treated as external cards and can be removed in the context menu for each card.
- A toast notification will now show when a new version is available to download.
- A toast notification will now show when the cropper has finished working, giving details on what work was done.
- A toast notification will now show when there were errors when rendering a pdf.
- A toast notification will now show when the MtG Downloader plugin finished downloading.
- The backside offset option has been extended to also allow for vertical alignment.
- On common request, a donation option was added to the README.
- There are now two global options to choose how cards are sorted by default.
- When hovering a card image it will now show the name of the card, with some exceptions.
- A new `Render to Png` checkbox is now available that is the same as changing the old `Render Backend` option to `Png`.
- You can now split front- and backside pages into separate pdfs with the new `Separate Backsides-PDF` option.
- The app can now also load `.webp` files.
- An additional executable `proxy_pdf_cli` is available to run a full project cycle from the command line.

### Changed
- The render button now specifies `Render PDF` or `Render PNG` based on what setting the user has selected.
- The default output format is now set to Jpg at 100% quality.
- The base pdf is now also offset for backsides, which allows e.g. cutting from the back when using an autoplotter.
- When the MtG Downloader is done, the `Cancel` button will be relabled to `Close` to reduce confusion.
- The threshold for showing the bad aspect ratio warning was increased, reducing how often users get the warning when it's so miniscule they likely won't be able to notice distortion.
- The bad aspect ratio warning was changed to a bad rotation warning in cases where the ratio is the inverse of the expected ratio.
- Cards will now maintain their chose amount even when hidden (e.g. when they are declared a backside).
- Backsides will now unhide when the backside setting is disabled.

### Removed
- The `Render Backend` option was removed in favor of a `Render to Png` option.
- The `LibHaru` dependency was removed as it didn't have any benefits over the now default `PoDoFo` backend.
- The button to reset backside has been moved to a card's context menu.

### Fixed
- The window title now shows the correct name.
- Only 16 log files will be kept now, as opposed to the previous 256.
- When writing a .dxf file all cards will now be in the right positions.
- When writing a .dxf file a header with units will be included that will allow some apps to import the file with correct sizes.
- The print preview will now update when cards are added or removed at runtime.
- The app will no longer sometimes crash when a crop job needs to retry.
- The `Guides Thickness` will now be respected in the preview, as good as possible given technical restrictions.
- The guides are no longer sized incorrectly on first time opening the print preview.
- Changing some settings no longer causes unnecessary hiccups in the app.

## [0.15.0] - 2025-07-10

### Added
- New auto backside setting that makes front-back matching automatic based on file names.

### Changed
- Run cropper and pdf generation in a configurable thread pool for much faster turnaround.

### Fixed
- Increased tolerance for bad aspect ratio warning.

## [0.14.4] - 2025-27-09

### Fixed
- When adding a new paper/card size the initial values will now respect system locale
- When attempting to delete a paper/card size the

## [0.14.3] - 2025-27-09

### Fixed
- Fixed some precision issues with guides offset.
- Fixed other precision issues with extended guides.

## [0.14.2] - 2025-17-09

### Changed
- Extended guides are no longer hidden behind `Advanced Mode`.

### Fixed
- Backsides are hidden correctly again once selected.
- The preview will now correctly ignore the `Rounded` card corners when bleed is non-zero.
- The app will no longer experience a crash when custom sorting has been saved with images that were since deleted.

## [0.14.1] - 2025-11-09

### Changed
- The MTG decklist downloader now also supports MODO, MTGA, and simple card name lists.
- After downloading images from Scryfall the corners will be filled with some good approximation that should not be visible after rounding corners.

### Fixed
- The app will no longer crash when switching image folders in some corner cases.
- The image browser wll no longer have weird layout when less than six images are available.
- On the default backside, the spinner will now correctly disappear when changing the backside.
- When the cropper cleans up files when an input file is deleted it will now also clean up uncropped files.
- The MPCFill downloader will no longer fail random downloads with big lists.
- The MPCFill downloader will now use the correct backsides for each card.

## [0.14.0] - 2025-07-09

> [!NOTE]
> Many features are now hidden behind the `Advanced Mode`. Be sure to check that if you are missing anything!

### Added
- A new global option named `Advanced Mode`, defaulting to off, that is used to hide a bunch of advanced options. Note that this only hides options, it does not reset any options to defaults.
- Two popups for editing, adding, and removing paper and card sizes have been added.
- In the preview you can now sort cards via drag-n-drop. The alphabetical order can also be restored with a button on top of the preview.
- After generating previews the user will now be notified of images that have bad aspect ratios and will thus lead to distorted images in the final output.

### Changed
- Maximum values for margins are now more sensible in `Full` and `Linked` control.
- Cards are now always centered within the available margins.
- Browsing for images with native file dialogs has been replaced with a dedicated dialog in order to avoid confusion when users selected images outside of their images folder.

### Fixed
- In `Full` margins control, when reaching margins so tight that only one card fits the opposing margins will be moved to always allow a full card.
- The cards border appears again in the preview when the `Export Exact Guides` option is enabled.
- Extended guides are now positioned correctly with assymmetric margins.
- Exported exact guides are now correct again with assymmetric margins.
- The options are is now sized correctly to avoid vertical scrolling on initial startup.
- Combo boxes and spin boxes inside the options area now consume mouse wheel input when focused, aka selected by the user.
- Shortly flickering windows that could appear on startup and in some rare corner cases no longer appear.
- The default `Poker` card size has been fixed to have correct units.

## [0.13.0] - 2025-19-08

> [!NOTE]
> Be sure to check the changelog as this version has some important changes!

### Added
- New options to control margins were added:
    - `Auto`: Automatically compute the margins to be as small as possible.
    - `Simple`: Allow changing top-left margins only, giving the option to offset the cards.
    - `Full`: Gives control over all margins, allowing to offset the cards and also reduce the amount of cards on the page.
    - `Linked`: Gives control over a single value and forces all margins to be that value. Useful for example when working with setups where specifications require specific margins.
- When any option other than `Auto` is chosen for  margins the final margins are now visible in the print preview.
- The option `Card Orientation` is now available to allow for more layout control:
    - `Vertical`: Same as before, cards will be upright.
    - `Horizontal`: Cards are rotated by 90 degrees.
    - `Mixed`: First fill out as much as possible with vertical cards, then try to fill the rest of the space with horizontal cards.

### Changed
- Users should now place all images, with or without bleed edge, into the images folder. The app should then use the available information to determine whether the image has bleed edge or not.
- The MtG decklist downloader will now automatically change the relevant settings instead of warning the user of invalid settings.
- When downloading cards via the MtG decklist downloader the respective card backs will also be automatically downloaded and assigned to each card.

### Removed
- The `Allow Precropped` setting is no longer needed and has been removed.

### Fixed
- The MtG decklist downloader will no longer fail downloading cards with non-numeric collector numbers.
- The card size `Standard x2` is now treated as a valid option for the MtG decklist downloader.
- Extended guides are now rendered the same in the preview and the rendered pdf.
- A rare crash during shutdown was fixed.
- Folders are now always treated as absolute, which should fix issues with using image folders on other drives.
- The `Rounded` card corners option now also works when exporting pdf files with embedded jpg files.

### Contributors
- Big props to @ahernandezjr for contributing the code for margin control.

## [0.12.2] - 2025-03-08

### Added
- Common paper size option Ledger is now integrated into the app.

### Fixed
- The Legal paper size is now correctly oriented.
- The app will no longer crash on various operations that it performs on accidentally empty images.
- The app will no longer crash when working with images that can not correctly have their DPI calculated or somehow have a DPI of 0.
- Fetching front-sides of double faced cards from Scryfall will no longer fail.

### Contributors
- Thank you @ahernandezjr for contributions to improve general stability, the Ledger paper size, and fixes to the Legal page size.

## [0.12.1] - 2025-23-07

### Added
- Common paper size options A4+ and A3+ are now integrated into the app.
- Binaries for Ubuntu-ARM64 are now available.

### Fixed
- The app will not crash anymore when rendering a PDF with images that contain an alpha channel while having rounded corners enabled.

## [0.12.0] - 2025-12-07

### Added
- Rudimentary decklist downloader can be used to download MPCFill, Moxfield, and Archidekt lists, found via `Global Options` -> `Plugins`.
- A new option `Card Options` -> `Corners` that allows you to export a pdf with rounded corners.
- A new option `Print Options` -> `Flip On` adds possibility to flip the output on the top edge as opposed to usually on the left edge.
- A new option `Guides Options` -> `Corner Guides` allows disabling cutting guides in the corners of cards.

### Changed
- Renamed card size options to something more general. **Note: This change required the global config.ini to be reset**
- The `Guides Options` -> `Export Exact Guides` option now also generates a `.dxf` file.
- Extended guides will now only be rendered outside of the card area.
- Split spacing into two values for vertical and horizontal, by defaulting linking them to be same.
- Logically linked some more options, aka disabling them when other dependent options are disabled.

### Fixed
- Guides in the preview are now correct with non-filled pages.
- Png backend will now successfully generate images with all page sizes.
- Page and card sizes are recomputed on all relevant setting changes.
- Now folders that are not exactly inside the working directory (typically next to the executable) can be used as image folders.
- Images that contain alpha channels are read as-is, instead of discarding the alpha channel.
- Enabling both extended guides and cross guides will no longer show the print preview incorrectly.

## [0.11.0] - 2025-26-05

### Added
- You can now select in the UI how images are written to the pdf. This lets you pick jpg to reduce output file size. 

## [0.10.2] - 2025-26-05

### Fixed
- The config is saved with the correct filename.

## [0.10.1] - 2025-26-05

### Fixed
- Cards that are picked as backsides are now correctly removed from the output.

## [0.10.0] - 2025-26-05

### Added
- The program now logs work being done to a file
- You can now control guides thickness and length with an option
- The units in which measurements are displayed can now be changed via the global units option
- Guides can be set to full crosses instead of cross segments
- Cards can be separated in the final output via a spacing option

### Changed
- The different options sections are now collapsible
- To use custom margins you now have to tick a box first
- The option to enable guides on backsides is now on the guides section  
- The corner weight option was removed in favor of a guides offset option

### Fixed
- Custom margins now correctly offset backsides in the opposite direction
- The print preview will now respect the extended guides option
- Uncrop option will no longer override source images when settings change
- The included cards sizes for MtG cards now have the right corner radius (requires deleting config.ini to take effect)
- The included cards sizes for MtG novelty cards is now correct (requires deleting config.ini to take effect)

## [0.9.4] - 2025-08-05

### Changed
- The uncrop option is now doing a nicer version that works alright for borderless cards
- The output folder now has a reasonable name when generating with bleed edge
- The cropper has received a tiny speedup

### Fixed
- Cropped files are written with the correct size again, so the embedded DPI information is correct
- The cropper will no longer randomly fail reporting to be finished when uncropping is enabled
- The program can no longer be closed in the middle of rendering a PDF to avoid a crash
- Previews will no longe rbe regeerated on every startup
- When running with uncrop option the cropper will no longer get stuck in an infinte loop of cropping

### Removed
- A deprecated option that was inaccessible and would not affect the program was removed

## [0.9.3] - 2025-24-04

### Fixed
- Card size on pdf when generating with bleed edge is now correct again

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
