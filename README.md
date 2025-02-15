# Proxy PDF Maker
Proxy PDF Maker is an app for creating PDF files for at-home printing of TCG Proxies. It handles cropping of bleed edges, alignment on pages, cutting guides, matching backsides, and more. This repo is a complete rewrite of [print-proxy-prep](https://github.com/preshtildeath/print-proxy-prep) app, which was written in Python. This app is instead written in C++ to get better performance, and allow easy distribution of binaries.

# Installation

TODO...

# Running the Program
First, throw some images with bleed edge in the `images` folder. Note that images starting with `__` will not be visible in the program. Then, when you open the program or hit the `Run Cropper` button from the GUI, it will go through all the images and crop them. From there you can start to setup your page.

## Cards
The left half of the window contains a grid of all cards you placed in the `images` folder. Below each image is a text input field and a +/-, use these to adjust how many copies for each card you want. On the top you have global controls to +/- all cards or reset them back to 1.

## Print Preview
On the top-left you can switch over to the `Preview`, which shows you a preview of the printed page. It should update automatically when you change printing settings on the right.

## Options
The right panel contains all the options for printing. Those that are self-explanatory (i.e. PDF Filename, Paper Size, Orientation) are skipped here.

### Enable Guides
Enables cutting guides, by default those are black-white guides. They are always in the corners of the cards to mark the exact size of a card.

#### Extended Guides
Extends the cutting guides for the cards on the edges of the layout to the very edge of the page, will require a tiny bit more ink to print but makes cutting much easier.

#### Guides Color A/B:
Guides are dashed lines made from these two colors. By default these are black and light gray. Choose colors that fit best for the cards you are printing.

### Bleed Edge
Instead of printing cards perfectly cropped to card size this option will leave a small amount of bleed edge. This emulates the real printing process and thus makes it easier to cut without having adjacent cards visible on slight miscuts at the cost of more ink usage.

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


# Troubleshooting:
- If you need support for a new feature, please open an Issue. Press F1 to get an about window, this contains information about the program and a link to the issues page. If you report an issue include a screenshot of this window.
- The program will automatically save if you close the window. It will not save if it crashes! The data is stored in `proj.json`.
- `proj.cache` is a file that stores data for the thumbnails.
- If the program crashes on startup first try to delete these two files, if that doesn't do it open an issue.
- When opening an issue to report a bug, please attach a zip file containing your `images` folder. `proj.json`, and `proj.cache`. Also include a screenshot of the F1 window if you can still start the program.
