### Tartarus-Driver
<pre>Kernel-space driver for the Razer Tartarus v2</pre>

## Tasks
- ~~Basic module structure~~
- ~~Parsing raw events~~
 - Fix dropped keys in event processing (many simultaneous key actions)
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

I've never written a device driver before, so when the need arose, I was optimistic to use this as a learning experience. I certainly have much to learn about the Linux kernel and its API, but I think I've made a pretty good guess using what resources I could find!  

The goal of this driver is to recreate some of the primary functionality that Razer Synapse provices to Windows users. Notably, device features such as customizable keymaps, profile hotswapping, and hypershift mode are of note. With just a couple KB of memory, we can construct a table of keybinds to map a device input to an interchangable output. Swapping the profile is (on paper) as generally as simple as changing the index to the table, and swapping out active keys.  

Macros are the primary feature that I've omitted. Ultimately, I don't use them personally so the motivation was limited, and functions with an indefinite run duration felt..._ambitious_ for kernel space. As such, I propose that the device should send the KEY_MACRO_X event to the kernel, where the parsed macro can be handled by its own process in user space. **(TODO)**  

## Requirements
- linux >=3.0 (?) + standard build tools (linux-headers, gcc, make, git, etc.)
- _DKMS_ : **TODO** Chosen method of module installation (There exist other ways that I do not yet know, feel free to make a PR!)
- python >= 3.11 : Allows profile customization GUI
- _<service system> : **TODO** I plan to eventually provide openrc/dinit service files for the user-space profile tool

## Profiles
**TODO:** Needs an explanation of the bind types, values, and many pretty pictures  
Also describe the nuances of profiles/hypershift keys (when they swap versus override)  
Once I add functionality to the mouse wheel, describe that too  
