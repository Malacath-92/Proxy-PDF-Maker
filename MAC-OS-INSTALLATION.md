# Installation on macOS
On newer macOS systems, namely macOS 15+, it is not possible to run unsigned apps without jumping through some hoops. The process for this is outlined below. Be sure you only do this with apps you trust!

Once you have extracted the app and try to start it, you will see this:
<p align="center">
    <img src="./readme_images/not_verified.png" alt="Not Verified" width=400/>
</p>

This is an indication that the app is not signed (because it requires a yearly fee to do this and the maintainer of this repo doesn't even have access to a Mac device to test on). If you trust that the app is not dangerous you can go to the system settings and allow the appl to launch. This is done in the "Security" section of "Privacy and Security", which looks like this:
<p align="center">
    <img src="./readme_images/allow_to_launch.png" alt="Allow to Launch" width=400/>
</p>

This will prompt another pop up asking you if you're sure, press "Open Anyway" in this popup:
<p align="center">
    <img src="./readme_images/open_anyway.png" alt="Open Anyway" width=400/>
</p>

Finally you will be prompted to insert your user and password to finalize the process:
<p align="center">
    <img src="./readme_images/password.png" alt="Password" width=400/>
</p>

At this point you should be able to open the app without problems.