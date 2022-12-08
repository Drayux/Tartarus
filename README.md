### Tartarus-Driver
<pre>Open source Linux driver for the Razer Tartarus V2</pre>

#### IMPORTANT NOTE!
The original purpose I had for making this driver was a result of giving up in my attempt to use the OpenRazer driver to configure my device keybinds. None of the applications built with the API that were listed on the page had support for keybinds, barring just one which failed to run on my system. Further, I could not for my life find any documentation on OpenRazer's API and was convinced that it in of itself did not support custom keymaps.  
  
Alas, I decided that my best option was going nuclear, and simply writing my own driver. As I began researching and learning, I became more and more comfortable with what the source code for drivers looked like. I discovered the 'razerkbd' module on my system and out of curiousity, researched the author, ultimately leading me back around to the ![OpenRazer Project](https://github.com/openrazer/openrazer/), having forgotten that it was on my system.  
  
Digging into the source, I've seen that the driver does a vast majority of exactly what I'd sought to built, and notably, has the knowledge of the Razer device communication specification. As such, some functions heavily cite this source! This project is somewhat an experimental sandbox for my first dive into drivers, and understanding how everything pieces together.  
  
Other references of interest:  
![hid-tmff2 (Thrustmaster Wheel HID Driver)](https://github.com/Kimplul/hid-tmff2)  
![hid-saitek_x52](https://github.com/nirenjan/libx52/blob/482c5980abd865106a418853783692346e19ebb6/kernel_module/hid-saitek-x52.c#L124)  
![Linux Kernel](https://github.com/torvalds/linux)  

### About
This is my first attempt at a driver, so who knows what will become of this repository!  
  
Ideally, the final stage for this driver will replace any default driver for the Tartarus.  
It should function as a standard keyboard would, with the primary exception of reading the keybind configuration from a file.  
  
Admittedly, this will be a big learning experience for me, as I know little enough about drivers to fully understand if what I'm hoping this will do will be viable.  

### Features
~~it just works~~

### Requirements
I may be missing something, but my attempt at listing the requirements to build this is as follows:

- linux-headers (standard package in about any distro...unsure if this is sufficient for alternative kernels)
- make
- linux (version >=3.0 I think...probably?)

### Installation
Currently pending an install script/AUR package/pacman hook (sorry if you're on other distros, I know very little about their package managers)  
  
1) Clone this repo into a directory of your choosing  
2) `cd` into the aforementioned directory  
3) Run `make` (this will compile the driver for your kernel)  
4) Run `insmod tartarus.ko` (this will usually require `sudo`)  
5) Enjoy!  

### Uninstallation
1) Run `rmmod tartarus` (this will usually require `sudo`)  
2) Optionally, delete the directory to which you cloned this repository  

### Configuration
Currently, the only way to configure the keymap with this driver is by editing the default map in the source:  
This can be found within the probe function (`tartarus_probe()`)  

### Tips/Tricks
- List installed driver modules with `lsmod`
- ^^You can search for this driver specifically with `lsmod | grep tartarus` (no output means it is not installed)
- This driver logs basic data to the kernel log, run `dmesg` to see this (usually requires invocation with `sudo`)
- Running `uname -r` will return your current Kernel version (this is what the makefile uses to determine which build system to use)

