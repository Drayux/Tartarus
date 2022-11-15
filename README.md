### Tartarus-Driver
<pre>Open source Linux driver for the Razer Tartarus V2</pre>

#### IMPORTANT DISCLAIMER!
The original purpose I had for making this driver was a result of giving up in my attempt to use the OpenRazer driver to configure my device keybinds. None of the applications built with the API that were listed on the page had support for keybinds, barring just one which failed to run on my system. Further, I could not for my life find any documentation on OpenRazer's API and was convinced that it in of itself did not support custom keymaps.  
  
Alas, I decided that my best option was going nuculear, and simply writing my own driver. As I began researching and learning, I became more and more comfortable with what the source code for drivers looked like. I discovered the 'razerkbd' module on my system and out of curiousity, researched the author, ultimately leading me back around to the !(OpenRazer Project)[https://github.com/openrazer/openrazer/], having forgotten that it was on my system.

Digging into the source, I've seen that the driver does a vast majority of exactly what I'd sought to built, and notably, has the knowledge of the Razer device communication specification. As such, MOST OF THE CODE HERE IS TAKEN DIRECTLY FROM OPENRAZER! This project is somewhat an experimental sandbox for my first dive into drivers, and understanding how everything pieces together.

### About

This is my first attempt at a driver, so who knows what will become of this repository!<br><br>
Ideally, the final stage for this driver will replace any default driver for the Tartarus.
It should function as a standard keyboard would, with the primary exception of reading the keybind configuration from a file.<br><br>
Admittedly, this will be a big learning experience for me, as I know little enough about drivers to fully understand if what I'm hoping this will do will be viable.

### Requirements

I may be missing something, but my attempt at listing the requirements to build this is as follows:

- linux-headers (standard package alongside kernel...unsure if this is sufficient for alternative drivers)
- make

### Features

TODO
