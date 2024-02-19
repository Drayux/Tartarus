### Tartarus-Driver
<pre>Kernel-space driver for the Razer Tartarus v2</pre>

## Tasks
- ~~Basic module structure~~
- ~~Parsing raw events~~
 - ~~Fix dropped keys in event processing (many simultaneous key actions)~~
- Device profiles
 - ~~Sysfs entries for user-space interfacing~~
 - ~~Profile indicator lights (needs URB refactor)~~
 - Python program to create and edit profiles
 - *System service to load profiles automatically (possibly with executable swapping)
- ~~Hypershift mode~~
- ~~Profile swapping~~
- Macros (key support)
- Handle "mouse" events
 - Swap device profiles with mouse wheel (needs implementation specifics--should mouse profiles be independent?)
- Setup DKMS

## About
About a year ago, I decided to officially throw in the towel on Windows. I knew there would be a couple jarring changes when moving to a world with a general lack of proprietary support, but the Linux experience has absolutely made it worth it. Alas, one of these challenges is a gaming perhipheral I swear by: The Razer Tartarus. This is a little half-keyboard device is a programmable macro pad with fantastic ergonomics, and I have found it an essential element in my gaming environment. Of course, using it on Linux is not so simple. All of the device functionality is handled in user space by an application with exclusive Windows support, and the device itself has no onboard keymap storage.  

I've never written a device driver before, so when the need arose, I was optimistic to use it as a learning experience. I certainly have much to learn about the Linux kernel and its API, but I think I've made a pretty good guess using what resources I could find!  

The goal of this driver is to recreate some of the primary functionality that Razer Synapse provides to Windows users. Notably, device features such as customizable keymaps, profile hotswapping, and hypershift mode are of note. With just a couple KB of memory, we can construct a table of keybinds to map a device input to an interchangable output. Swapping the profile is (on paper) as generally as simple as changing the index to the table, and swapping out active keys.  

Macros are the primary feature that I've omitted. Ultimately, I don't use them personally so the motivation was limited, and functions with an indefinite run duration felt..._ambitious_ for kernel space. As such, I propose that the device should send the KEY_MACRO_X event to the kernel, where the parsed macro can be handled by its own process in user space with a program such as [Wootomation](https://github.com/WootingKb/wooting-macros). **(TODO)**  

Included in this repo is a half-ass Python program to provide a GUI for editing profiles. That said, this is a very barebones script to save me the headache of spending time programming in python. Hopefully it may serve useful as a starting point or basic configuration tool.  

NOTE: Being a WIP, there are still some funny quirks you may notice:  
- The device defaults to a "debug profile" where buttons 01 - 05 will swap the respective profile  
- The default maps at profiles 2 and 3 are the maps that I regularly use for gaming, which may prove unusual to many  
- The scroll wheel does not work at all--It's one of the last features I expect to add  

## Requirements
- linux >=3.0 (?) + standard build tools (linux-headers, gcc, make, git, etc.)
- _DKMS_ : **TODO** Module installation (There likely exists alternatives; feel free to make a PR!)
- python >= 3.11 : Allows profile customization GUI
- _\<service system\>_ : **TODO** I plan to eventually provide openrc/dinit service files for the user-space profile tool

## SysFS
The keyboard interface (inum 0) will generate three sysfs entries: `profile_count`, `profile_num`, and `profile`  
All of these entries are found under `/sys/bus/hid/drivers/hid-tartarus/<dev path>/`  

Every interface will additionally generate the file `intf_type`  

### `profile_count`
> READ ONLY  
The number of profiles supported by the device is determined at compile-time by the macro `PROFILE_COUNT` in `module.h`  
`cat profile_count` will display this value (currently 8 by default)  

### `profile_num`
> READ / WRITE  
Represents the active device profile (base 10)  
Use `echo -n "3" > profile_num` to set the device to profile 3; The function will perform bound checking  

### `profile`
> READ / WRITE  
Represents the data of the active device profile (raw)  
The profile structure will likely evolve from the time of writing this, however it is currently an array of 512 bytes: every pair of 2 bytes represents one keybind within a profile, indexed by the raw key value  
Reading/writing from this file will output/overwrite the profile of the _active_ profile respectively.  
`cat profile | hexdump -C` is one such way of viewing a profile's data 

### `intf_type`
> READ ONLY
Outputs a string allowing a user-space program to determine which interface is which  
_NOTE: There is probably a better way to do this that I do not yet know about; feel free to PR!_

## Profiles
**NOTE:** The python configuration tool "linapse" must be ran as sudo to actually change the profile in the driver. Truthfully, I do not know what the protocol I should use here is. Though this may prove problematic for users on multi-user machines.  
**TODO:** Needs an explanation of the bind types, values, and many pretty pictures  
Also describe the nuances of profiles/hypershift keys (when they swap versus override)  
Once I add functionality to the mouse wheel, describe that too  
