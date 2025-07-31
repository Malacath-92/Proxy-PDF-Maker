# Why use a Cutting Machine

Many players making proxies for their favorite game (or even their own card games) use a guillotine or rotary cutter, a corner cutter, maybe even scissors. All those methods are very functional and give good results after getting used to them. However a cutting machine or plotter can give much more consistent results. Thus many players like to cut their proxies with a cutting machine.

There are a multitude of automatic cutting machines available to end-users. Common ones are provided by _Silhouette America_, _Cricut_, or _Siser_. They all use their own software for creating designs, in this tutorial we will focus on _Silhouette Studio_ and _Cricut Design Space_, which are the design apps used for Silhouette America and Cricut respectively, but it should be possible to translate the few steps within the design app to other brands.

## Prerequisites
Make sure you downloaded the latest version of [Proxy-PDF-Maker](https://github.com/Malacath-92/Proxy-PDF-Maker/releases).

Additionally you will have to install Visual Studio Redistributable: https://aka.ms/vs/17/release/vc_redist.x64.exe

Do the usual steps to create your project, use the preview to verify everything would print as you want.  If you want to follow along this tutorial exactly, download the images in the folder `tutorial_cards` folder and load the project `sample_project.json`.

## Setting up the _Silhouette Studio_ Project

To setup the project we need to generate a the cutting guides from the app. We do this by disabling guides and enabling `Export Exact Guides` in the `Guides Options` section.

<p align="center">
    <img src="./images/exact_guides.png" alt="Exact Guides"/>
    <br>
    Disabled guides but exporting exact guides
</p>

Now we can press `Render Document`, this will generate a `_printme.svg` file which has the exact card guides. In addition we should have a folder `_printme` with a file `0.png` which contains the first page's image.

Now we can open a new project in _Silhouette Studio_ and choose the right page setup. Now we can drag-n-drop the `_printme.svg` file into the project (or use an online tool to convert it to `.dxf` and drop that in instead). Copy the `Cards Size` values over to the size of the imported cutting guides, press the `Center to Page` option.

<p align="center">
    <img src="./images/silhouette_page_setup.png" alt="Page Setup"/>
    <br>
    Page setup in Silhouette Studio for the sample project
</p>

<p align="center">
    <img src="./images/cards_size.png" alt="Cards Size"/>
    <br>
    Cards size in app, note that this respects spacing and bleed options correctly
</p>

<p align="center">
    <img src="./images/silhouette_guides_setup.png" alt="Silhouette Guides"/>
    <br>
    Resizing and centering guides in Silhouette Studio
</p>

Next select the `Print & Cut` option, enable registration marks and set them up so that the cross-hatched area does not overlap the cutting guides as little as possible. At this point we can print the document to a pdf, name it `cutting_base.pdf` and save it to the `res/base_pdfs` folder.

<p align="center">
    <img src="./images/silhouette_print_and_cut.png" alt="Print & Cut"/>
    <br>
    Print & Cut settings for the sample project
</p>

While we are here we can also disable the outer-most cutting line. Right-click the outlines and choose `Release Compound Path`, then select only the outer guides and delete them. Now select all the guides again, right-clock and choose `Make Compound Path`.

<p align="center">
    <img src="./images/silhouette_release.png" alt="Release Compound Path"/>
    <br>
    Splitting cutting guides into separate paths
</p>


## Generating the full PDF in the App

>! Note:
>! You currently have to restart the app for this step, but that will be fixed in the future

First go to the `Global Config` and choose `PoDoFo` for the `Rendering Backend` option.

Now we change `Paper Size` to `Base Pdf`, which then shows another dropdown which we want to choose our pdf from, i.e. `cutting_base`. This will generate all the pages on top of the base pdf, so the registration marks are automatically there. They will also show up on the backside, but we can't cut with the backside up as we probably have to fix the offset to align correctly.

<p align="center">
    <img src="./images/base_pdf.png" alt="Base Pdf"/>
    <br>
    Setting up the Base Pdf option
</p>

Since we centered the cutting guides on the page we don't need to do anything else. If you however moved the guides on the page, be sure to check `Custom Margins` and paste the margins from within _Silhouette Studio_ into the margins field (`X` into `Left Margin` and `Y` into `Right Margin`).

Lastly you can now press `Render Document` to generate the pdf.

## Doing the Print & Cut

Lastly you just print as usual, laminate, sticker, whatever you like. Then you place your prints on the mat, insert into your machine and send the cutting instructions. The actual settings, tools, and method for cutting highly depend on your material and machine, so we won't go into details about that here.

## _Cricut Design Space_ Specific Instructions

### Initial Setup

Follow the Silhouette Studio instructions up to rendering the document. Make sure you only have `Export Exact Guides` enabled under `Guides Options`. It's recommended to set margins to at least 1.25 cm, but we recommend setting margins to around ~2.5 cm for reasons explained later.

### Creating the Cricut Project

1. Open a new project in _Cricut Design Space_ and upload the `_printme.svg` file
2. Click on the Cut Guides, and in the top bar, set Operation to 'Print Then Cut' and unlock the Size
3. Copy the values from `Cards Size` within the print options
   > **Note:** You may have to go to Settings and Canvas and set Units to Metric. The size in the bar will be in centimeters, so for example, 268.0 mm should be set to 26.8 within Cricut

### Configuring Page Settings

1. Go to Settings and select the Load Type menu
2. Set Print Then Cut Page Size to Tabloid
   > **Important:** The Load Type setting will use whatever mat you decide to use which is important because Cricut must recognize the mat length. Ensure the mat fits the paper

### Setting Up the Print Area

1. Click Make and set the box in the Upper Leftmost corner
2. If you have lots of margin space, leave the box as is for easy future replication

### Configuring Print Settings

1. Click Continue
2. Under 'Printed', click 'Send to Printer' and select Printer: Microsoft Print to PDF
3. If you are not using tabloid/ledger paper, also click 'Use System Dialog'
4. Click Print
5. In the System Print pop up:
   - Set Printer as Microsoft Print to PDF
   - Click Preferences and select the proper orientation
   - Click Advanced and set the proper paper type
   - Click OK and Print

### Finalizing the Base PDF

_This section is a summarization fo the 'Generating the full PDF in the App' section._
1. Save/Move the file to `res\base_pdfs` within the PDF Proxy Printer folder
2. Restart PDF Proxy Printer
3. Reopen and set Rendering Backend to PoDoFo
4. Set Paper Size to Base Pdf and set Base Pdf to your proper file
5. Set the margins to what you set them to previously

### Fine-tuning Alignment

1. Click Render Document and look at the resultant PDF
2. Continue adjusting margins until the lines are perfectly aligned
3. Save the final PDF

### Printing and Cutting

Print the PDF with full page and insert it into the Cricut. Follow the onscreen instructions.
