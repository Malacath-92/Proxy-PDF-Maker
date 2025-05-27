> [!NOTE]
> All sample images for this have an input bleed edge, be sure to pay attention to the [Notes on Bleed Edge](#notes-on-bleed-edge) section to verify and correctly handle your images

# LiberProxies

LiberProxies is a EU-based print-service that offers high-quality proxies at reasonable prices. Generally it is enough to choose cards and a set they were printed on to order your proxies. It is also possible to order prints of custom-created proxies by just sharing those as image files. In case of requesting custom-created double-sided cards however LiberProxies requires that you share a print-ready document with them.

This tutorial focuses on creating such print-ready documents.

## Prerequisites
Make sure you downloaded the latest version of [Proxy-PDF-Maker](https://github.com/Malacath-92/Proxy-PDF-Maker/releases).

Then collect all images of cards you want to order and a backside you like. Rename the backside image to `__back` in order to simplify later steps.

If you want to follow along this tutorial exactly, download the images in the folder `cards` right next to this document.

### Notes on Bleed Edge
Ideally your input images should have a 3mm bleed edge, which means they should be 3mm bigger than a printed card. To find out if they do you can check the dimensions of the image, divide the width by the height:
- if the ratio is roughly `0.74` then the image has a 3mm bleed edge,
-  if the ratio is roughly `0.72` then it does not have a 3mm bleed edge,
- and if it has any other ratio your image will be stretched or squished, you should find a better image.

<p align="center">
    <img src="./images/bleed_example.png" alt="Bleed Example"/>
    <br>
    Example calculation for a card that has a 3mm Bleed Edge
</p>

### Card Images

Next to the `proxy_pdf` executable, create a new folder and name it `liberproxies` and inside that folder create another folder named `crop`. Paste all your cards with a 3mm Bleed Edge into the `liberproxies` folder and all without into the `crop` folder.

## App Setup

Start up the `proxy_pdf` exectuable, it should look something like the below image.

<p align="center">
    <img src="./images/first_startup.png" alt="First Startup"/>
    <br>
    If you have used the app before it may already look different
</p>

The settings should be visible on the right side of the screen, go ahead and collapse all the sections by pressing the little arrow next to their names.

<p align="center">
    <img src="./images/options_collapsed.png" alt="Collapsed Options"/>
    <br>
    This is how it should look once you collapsed all options
</p>

We will now go through all the settings to adjust them to exactly what LiberProxies requires to print.

### Global Config

To start us off, expand the bottom section. Here we want to make sure to:
- set `Units` to `mm`,
- set `Rendering Backend` to either `LibHaru` or `PoDoFo`,
- set `Image Format` to `Jpg`,
- set `Jpg Quality` to `100`,
- tick `Allow Precropped` (technically only necessary if you have images without 3mm Bleed Edge),
- set `Color Cube` to `None` and
- set `Max DPI` to `800`.

All other options are irrelevant for us.

<p align="center">
    <img src="./images/options_global.png" alt="Global Config"/>
</p>

You can now collapse this section again.

### Card Options

Extend this section and set the following options:
- `Bleed Edge` to `1.5mm`,
- `Spacing` to `1mm`,
- tick `Enable Backside` and
- `Offset` to `0mm`.

<p align="center">
    <img src="./images/options_card.png" alt="Card Options"/>
</p>

Note the spinner above `__back.png`, this just means that the app currently can not find the image. We will fix that once we select the image folder. For now, collapse the section.

### Guides Options

Expand the section and set the following:
- Untick `Export Exact Guides`,
- tick `Enable Guides`,
- untick `Enable Backside Guides`,
- untick `Extended Guides`,
- tick `Cross Guides`,
- set both colors to pure white,
- set `Guides Offset` to `0mm`,
- set `Guides Thickness` to `0.1mm`,
- set `Guides Length` to `1mm`.

<p align="center">
    <img src="./images/options_guides.png" alt="Guides Options"/>
</p>

Now collapse this section.

### Print Options

Expand the section and set the following:
- Set `Output Filename` to whatever you like (this will be the name of your pdf),
- set `Card Size` to `Magic the Gathering`,
- set `Paper Size` to `A4`,
- untick `Custom Margins`,
- set `Orientation` to `Portrait`.

<p align="center">
    <img src="./images/options_print.png" alt="Print Options"/>
</p>

### Load Images

Now press `Set Image Folder` at the top and select the `liberproxies` folder. Now all the cards you have in that folder should start loading in. Observe also the loading bar at the top-right, which has to finish before being able to generate the pdf.

<p align="center">
    <img src="./images/with_images.png" alt="Loading Images"/>
    <br>
    Some images have already loaded, while some are still waiting
</p>

Each tile in this grid represents on card. The small image behind the card represents its backside. If you did not name your backside to `__back` then these may not load. If so, go back to the `Card Options` segement and press `Choose Default`, then navigate to the backside you want to use by default.

### Choosing Double-Sided Cards

For each card in this grid, locate the card that represents the front-side of a DFC. Click on the little image for its backside and choose the image representing its back-side. The tile representing the back-side should now disappear. If you made a mistake and want to reset a back-side just press on the little broom icon below the card image.

<p align="center">
    <img src="./images/backsides_selected.png" alt="Backsides Selected"/>
    <br>
    Only the front-sides of cards and single-faced cards are left after matching up
</p>

### Changing the Amount per Card

Lets say we want to order three copies of the `Plains` card. Simply use the `+` and `-` buttons below the card to increase or decrease the amount per card.

<p align="center">
    <img src="./images/more_cards.png" alt="More Cards"/>
</p>

### Generating the PDF

Finally we just have to press `Render Document` button in the top-right and wait for rendering to finish.

<p align="center">
    <img src="./images/generated_pdf.png" alt="Generated Pdf"/>
    <br>
    Once generated you should have a two-page PDF that contains all your proxies with front- and back-sides
</p>
