# Proxy PDF Maker
Proxy PDF Maker is an app for creating PDF files for at-home printing of TCG Proxies. It handles cropping of bleed edges, alignment on pages, cutting guides, matching backsides, and more. This repo is a complete rewrite of [print-proxy-prep](https://github.com/preshtildeath/print-proxy-prep) app, which was written in Python. This app is instead written in C++ to get better performance, and allow easy distribution of binaries.

# Installation

Go to the [Releases](https://github.com/Malacath-92/Proxy-PDF-Maker/releases) page and grab the latest version for your platform. Unzip this into any folder of your choice and you should be good to go.
Additionally you will have to install Visual Studio Redistributable: 	https://aka.ms/vs/17/release/vc_redist.x64.exe

# Running the Program
First, throw some images with bleed edge in the `images` folder. Note that images starting with `__` will not be visible in the program. Then, when you open the program or hit the `Run Cropper` button from the GUI, it will go through all the images and crop them. From there you can start to setup your page.

## Cards
The left half of the window contains a grid of all cards you placed in the `images` folder. Below each image is a text input field and a +/-, use these to adjust how many copies for each card you want. On the top you have global controls to +/- all cards or reset them back to 1.

## Print Preview
On the top-left you can switch over to the `Preview`, which shows you a preview of the printed page. It should update automatically when you change printing settings on the right.

## Options
The right panel contains all the options for printing. Those that are self-explanatory (i.e. PDF Filename, Paper Size, Orientation) are skipped here.

### Paper Size: Fit
The `Fit` option for paper size will fit exactly `A` times `B` cards, without any margins whatsoever. The choice of `A` and `B` is made in the option that will appear below the `Paper Size` option once `Fit` is selected. Check out the preview for an idea of how this ends up.

### Paper Size: Base Pdf
The `Base Pdf` option is only available with the `PoDoFo` render backend. It will load all pdf files inside the folder `res/base_pdfs` and present them in the drop-down below. When rendering the first page of the selected pdf will be used as a base for each page in the output. This is useful for example when using an automatic cutting machine to add registration marks to each page automatically.

### Cards Size
Gives you information about how big the cards will be once printed, this is the full grid per-page. Not the individual cards.

### Enable Guides
Enables cutting guides, by default those are black-white guides. They are always in the corners of the cards to mark the exact size of a card.

#### Extended Guides
Extends the cutting guides for the cards on the edges of the layout to the very edge of the page, will require a tiny bit more ink to print but makes cutting much easier.

#### Guides Color A/B:
Guides are dashed lines made from these two colors. By default these are black and light gray. Choose colors that fit best for the cards you are printing.

### Bleed Edge
Instead of printing cards perfectly cropped to card size this option will leave a small amount of bleed edge. This emulates the real printing process and thus makes it easier to cut without having adjacent cards visible on slight miscuts at the cost of more ink usage.

### Corner Weight
This option determines where the cutting guides will be when the bleed edge is bigger than zero. If the slider is all the way to the right the cutting guides will be exactly at the card corners, if it is all the way on the left it is in the corner of the printed image of each card. Open the preview while changing this setting to get a good idea of it.

### Enable Backside
Adds a backside to each image, which means when printing each other page will automatically be filled with the corresponding backsides for each image. This allows for double-sided cards, different card backs, etc.

The default backside is `__back.png`, if that file is not available a question mark will be shown instead. To change the default just click on the `Default` button and browse to the image you want.

To change the backside for an individual card, click on the backside for that card in the card grid and browse to the image you want.

#### Offset
In some cases one can't use Duplex Printing, either because the printer doesn't support it or the print medium is too thick. In those cases you'll have to manually turn the page between front- and backside prints. For many printers this will result in a slight offset between the two sides that is more or less consistent. Do a test print to measure this difference and insert it into the `Offset` field.

### Enable Oversized Option
Enables option to mark cards individually as oversized. Oversized and regular sized cards will be printed on the same sheet given space. Check the preview to verify printing.

### Display Columns
Determines how many columns are displayed in the card grid on the left. Smaller numbers are better for smaller screens.

### Rendering Backend
Choose here the backend for rendering the final output file. `LibHaru` and `PoDoFo` will render to a `.pdf` file, while `Png` will render to a set of png files. `Png` output is particularly useful when using the `Fit` paper size option, then using the result outputs for manually creating a print layout in another software.

### Allow Precropped
In some cases you may find yourself having card images that don't have a bleed edge. In those cases, enable this option and place your images into the `images/cropped` folder. The program will automatically add a black bleed edge so that all features of the program work as intended.

### Color Cube
Dropdown of all color cubes found in the folder `res/cubes`, which have to be `.CUBE` files with an arbitrary resolution. Higher resolutions will not slow down application of the cube maps, trilinear interpolation is used irrespective of resolution. Ships with the following color cubes:
- Foils Vibrance: When printing onto holographic paper/sticker/cardstock use this color cube to get a more vibrant looking result.

### Preview Width
Determines the resolution of previews in the card grid and the page preview. Smaller numbers result in faster cropping but worse previews.

### Default Page Size
Dropdown to choose the default page size when creating a new project. Page sizes can be configured in config.ini, the format has to be `width x height unit` where
- `width` and `height` are decimal number, with a period as decimal divider,
- the two values are divided by a single `x` surrounded by spaces
- and `unit` is one of `cm`, `mm`, or `inches`.

### Theme
Choose a theme from among all themes found in the folder `res/styles`, which have to be `.qss` files. Predefined themes are:
- Default (OS specific)
- Fusion
- Wstartpages
- Material Dark
- Darkeum
- Combinear

## Actions
At the top of the options you can see an `Actions` section, which are all buttons do perform various actions.

### Run Cropper
When adding new images, removing images, changing color cube, changing bleed edge, or changing preview resolution you will have to press this button to update previews, the card grid, and to render the document.

### Render Document
When you're done getting your print setup, hit this button and it will make your PDF and open it up for you. Hopefully you can handle yourself from there.

### Save Project
Saves your project (which includes _all_ settings except for those under `Global Config`), lets you choose any filename to save to such that you have multiple projects ready to go. The last saved project will be auto-opened the next time you start the app.

### Load Project
Loads a project previously saved via `Save Project`.

### Set Image Folder
Lets you choose a folder in which you have your images, this is saved with the project.

### Open Images
Opens the image folder for this project in a file explorer.

## Png Generation
An advanced feature that is still experimental is png generation. This may be useful for some software that assembles printouts in some other way, for example some cutting machines layout software. To enable this option open `config.ini` and set the following option:
```ini
PDF.Backend=Png
```
You may also optionally add the following option, with `N` being a value between `0` and `9`, where higher values result in slower rendering and smaller files:
```ini
PDF.Backend.Png.Compression=N
```
The files will now be output one page at a time into a folder specified by the `PDF Filename` option. These files contain information about the DPI, which is forced to the `Max DPI` option.

## Reducing PDF Size
With a lot of images a pdf file generated from this app can easily reach hundreds of MB in size, even a GB for a full commander deck. You can reduce this by embedding jpg images instead of png images in the pdf. Currently this is only available from `config.ini`. Add these two lines to get it working:
```
PDF.Backend.Image.Format=Jpg
PDF.Backend.Jpg.Quality=95
```
Here the quality value is a whole number between `1` and `100`, defaulting to `95`. Lower numbers will reduce file size at the cost of image artifacts appearing.

# Troubleshooting:
- If you need support for a new feature, please open an Issue. Press F1 to get an about window, this contains information about the program and a link to the issues page. If you report an issue include a screenshot of this window.
- The program will automatically save if you close the window. It will not save if it crashes! The data is stored in `proj.json`.
- `images/crop/preview.cache` is a file that stores data for the thumbnails.
- If the program crashes on startup first try to delete these two files, if that doesn't do it open an issue.
- When opening an issue to report a bug, please attach a zip file containing your `images` folder and `proj.json`. Also include a screenshot of the F1 window if you can still start the program.
