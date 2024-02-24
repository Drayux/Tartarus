import io
import os
import sys

from functools import partial

# I found this the most useful tkinter resource: https://www.pythontutorial.net/tkinter/
import tkinter as tk
from tkinter import ttk as ttk
from tkinter import filedialog as tkfiledialog

global DRIVERPATH_
global KBDPATH_
global MAXPROFILES_

class Bind:
    # Static methods to get the text of a bind
    def printTypeString(_type, shorten = False):
        match _type:
            case 0: return "NONE" if shorten else "DISABLE"
            case 1: return "KEY"
            case 2: return "SHIFT" if shorten else "HYPERSHIFT"
            case 3: return "PROFL" if shorten else "PROFILE"
            case 4: return "MACRO"
            case 5: return "SCRIPT"
            case 6: return "SWKEY"
            case 255: return "DEBUG"
            case _: return "UNDEF" if shorten else "UNDEFINED"

    # We don't automatically perform this in initalization since self._data can be reassigned by the UI
    def parseDataStr(self):
        # No conversion if our data is already processed
        if isinstance(self._data, int): return True
        
        comp = self._data
        self._data = 0

        # Unset data if no keybind
        if comp is None or len(comp) == 0: return True
        comp = comp.upper()

        # Check the list of string keycodes (only applies to key bind type)
        if self._type == 1:
            idx = -1
            for i, string in enumerate(Profile._codes):
                if string != comp: continue
                idx = i
                break

            if idx >= 0:
                self._data = idx
                return True

        # Check the list of macro keycodes (only applies to macro bind type)
        elif self._type == 4:
            idx = -1
            for i, string in enumerate(Profile._mcodes):
                if string != comp: continue
                idx = i
                break

            if idx >= 0:
                self._data = idx
                return True

        # Try to convert to hex/decimal
        try: comp = int(comp, 0)
        except Exception: return False
        
        # Range checks
        if comp < 1: return False
        if self._type == 1 and comp >= len(Profile._codes): return False
        elif self._type == 4 and comp >= len(Profile._mcodes): return False

        self._data = comp
        return True

    # Turns the data value into a more useful format (opposite of parse)
    def printDataStr(self):
        if not self.parseDataStr(): return ""
        match (self._type):
            case 0: return ""
            case 1:
                ret = None
                try: ret = Profile._codes[self._data]
                except IndexError: return "UNSUPPORTED"
                
                if ret is None: return "UNSUPPORTED"
                return ret
            case 2: return str(self._data)
            case 3: return str(self._data)
            case 4: 
                ret = None
                try: ret = Profile._mcodes[self._data]
                except IndexError: return "UNSUPPORTED"
                
                if ret is None: return "UNSUPPORTED"
                return ret
            case _: return str(self._data)  # TODO
    
    def __init__(self, _type, _data):
        self._type = _type if type(_type) is int else int(_type, 0)
        self._data = _data  #ord(_data)

    def __str__(self):
        ret = Bind.printTypeString(self._type)
        ret += " : "
        ret += f"'{self._data}'" if type(self._data) is str else hex(self._data)

        return ret

    def __eq__(self, other):
        if self._type == other._type and self._data == other._data: return True
        return False

class Profile:
    _size = 512     # Update upon modification of profile format
    _keys = [
        0x1E, 0x1F, 0x20, 0x21, 0x22,
        0x2B, 0x14, 0x1A, 0x08, 0x15,
        0x39, 0x04, 0x16, 0x07, 0x09,
        0x42, 0x1D, 0x1B, 0x06, 0x2C,
        0x44, 0x50, 0x52, 0x4F, 0x51
    ]

    # https://elixir.bootlin.com/linux/v6.7/source/include/uapi/linux/input-event-codes.h#L65
    # Currently maps keycodes 0 - 127
    _codes = [
        None,   "ESC",  "1",    "2",    "3",    "4",    "5",    "6",    "7",    "8",        #   0 -   9
        "9",    "0",    "-",    "=",    "BCKSP","TAB",  "Q",    "W",    "E",    "R",        #  10 -  19
        "T",    "Y",    "U",    "I",    "O",    "P",    "[",    "]",    "ENTER","LCTRL",    #  20 -  29
        "A",    "S",    "D",    "F",    "G",    "H",    "J",    "K",    "L",    ";",        #  30 -  39
        "\'",   "~",    "LSHFT","\\",   "Z",    "X",    "C",    "V",    "B",    "N",        #  40 -  49
        "M",    ",",    ".",    "/",    "RSHFT","KP*",  "LALT", "SPACE","CAPS", "F1",       #  50 -  59
        "F2",   "F3",   "F4",   "F5",   "F6",   "F7",   "F8",   "F9",   "F10",  "NUMLK",    #  60 -  69
        "SCRLK","KP7",  "KP8/U","KP9",  "KP-",  "KP4/L","KP5",   "KP6/R","KP+",  "KP1",     #  70 -  79
        "KP2/D","KP3",  "KP0",  "KP.",  "",     "ZEN",  "ANY",  "F11",  "F12",  "RO",       #  80 -  89
        "KAT",  "HIR",  "HEN",  "KAHI", "MUHEN","JP,",  "KPENT","RCTRL","KP/",  "SYSRQ",    #  90 -  99
        "RALT", "LF",   "HOME", "UP",   "PGUP", "LEFT", "RIGHT","END",  "DOWN", "PGDN",     # 100 - 109
        "INS",  "DEL",  "MACRO","MUTE", "VOLDN","VOLUP","POWER","KP=",  "KP+/-","PAUSE",    # 110 - 119
        "SCALE","KP,",  "HANGE","HANJA","YEN",  "LMETA","RMETA","MENU"
    ]

    _mcodes = [
        None,   "M1",   "M2",   "M3",   "M4",   "M5",   "M6",   "M7",   "M8",   "M9",       # 65(5) - 664
        "M10",  "M11",  "M12",  "M13",  "M14",  "M15",  "M16",  "M17",  "M18",  "M19",      # 665 - 674
        "M20",  "M21",  "M22",  "M23",  "M24",  "M25",  "M26",  "M27",  "M28",  "M29",      # 675 - 684
        "M30",  None,   None,   "START","STOP", "CYCLE","PRE1", "PRE2", "PRE3"              # 685 - 693
    ]

    # Populate the profile's data with keybinds parsed from the sysfs buffer
    def load(self, buf, size = 0):
        self.keymap = []
        
        i = 0
        while (i < Profile._size):
            btype = buf[i] if i < size else 0
            bdata = buf[i + 1] if (i + 1) < size else 0
                        
            self.keymap.append(Bind(btype, bdata))
            i += 2

    # Returns the buffer to store; does not perform the storing itself
    def store(self):
        if len(getattr(self, "keymap", [])) <= 0: return None

        i = 0
        buf = bytearray(Profile._size)
        for bind in self.keymap:
            if (i + 1) >= Profile._size: break
            
            buf[i] = bind._type
            buf[i + 1] = bind._data

            i += 2

        return buf, i

    # This will need to be updated if more fields are added to profiles
    def __repr__(self):
        length = len(getattr(self, "keymap", []))
        return f"<Profile : {length * 2} bytes>"

    def __str__(self):
        if len(getattr(self, "keymap", [])) <= 0:
            return "[ No data ]"
    
        ret = ""
        key_idx = 0

        for i in range(5):
            # Row 1 -> key number
            for j in range(5):
                key_num = (j + 1) + i * 5
                ret += f"┏╸》{'0' if key_num < 10 else ''}{key_num} ━┓\t"

            # Row 2 -> bind type
            ret += "\n"
            for j in range(5):
                key = Profile._keys[key_idx + j]
                btype = self.keymap[key]._type
                ret += f"┃ {Bind.printTypeString(btype, True)}\t┃\t"

            # Row 3 -> bind data
            ret += "\n"
            for j in range(5):
                key = Profile._keys[key_idx + j]
                btype = self.keymap[key]._type
                bdata = self.keymap[key]._data

                code = "<OOB>"
                try: code = Profile._codes[bdata]
                except IndexError: pass
                
                if (btype == 1): ret += f"┃ {code}\t┃\t"
                else: ret += f"┃ {hex(bdata)}\t┃\t"

            # Row 4 -> pretty formatting
            ret += "\n"
            for j in range(5):
                ret += "┗━━━━━━━┛\t"

            ret += "\n"
            if (i != 4): ret += "\n"
            key_idx += 5

        return ret

class Editor:
    class ProfileSelect(ttk.Frame):
        def __init__(self, master, callback):
            super().__init__(master, style = "ProfileSelect.TFrame")
            
            frameStyle = ttk.Style()
            frameStyle.configure("ProfileSelect.TFrame", 
                background = "#212220"
            )
        
            buttonStyle = ttk.Style()
            buttonStyle.configure("ProfileSelect.TButton", 
                foreground = "#eeddee",
                background = "#212220",
                font = ("System", 13)
            )

            self.columnconfigure(0, weight = 1)
            for x in range(MAXPROFILES_):
                self.rowconfigure(x, weight = 1)

            self.profbuttons = []
            for x in range(MAXPROFILES_):
                button = ttk.Button(self, 
                    style = "ProfileSelect.TButton",
                    command = partial(callback, x + 1)
                )
                button.grid(row = x, sticky = "nsew")
                self.profbuttons.append(button)

    class ProfileView(ttk.Frame):
        def __init__(self, master, load_callback, save_callback, key_callback):
            super().__init__(master, style = "ProfileView.TFrame")
            
            frameStyle = ttk.Style()
            frameStyle.configure("ProfileView.TFrame",
                background = "#24b01c"
            )
            
            buttonStyle = ttk.Style()
            buttonStyle.configure("ProfileView.TButton",
                foreground = "#eeddee",
                background = "#313630",
                font = ("System", 13)
            )

            self.rowconfigure(0, weight = 2)
            for x in range(5): self.columnconfigure(x, weight = 1)
            for x in range(5): self.rowconfigure(x + 1, weight = 4)

            # Operations buttons (save, load, TODO: settings)
            self.load = ttk.Button(self,
                text = "Load",
                style = "ProfileView.TButton",
                command = partial(load_callback)
            )
            self.load.grid(row = 0, column = 1, padx = 5, sticky = "ew")
            
            self.save = ttk.Button(self,
                text = "Save",
                style = "ProfileView.TButton",
                command = partial(save_callback)
            )
            self.save.grid(row = 0, column = 3, padx = 5, sticky = "ew")

            # Profile keys
            self.keybuttons = []
            for x in range(25):
                key = ttk.Button(self, 
                    style = "ProfileView.TButton",
                    command = partial(key_callback, x)
                )
                key.grid(row = int(x / 5) + 1, column = x % 5, padx = 20, pady = 20, sticky = "nsew")
                self.keybuttons.append(key)

    def __init__(self):
        self.pnum = 0
        self.profile = None
        self.lock = False       # Not a perfect mutex; disable buttons in general use cases
        # self.profiles = [None for x in range(MAXPROFILES_)]
    
        self.rootWindow = tk.Tk()
        self.rootWindow.title("Linapse Configurator")
        self.rootWindow.geometry("1200x768")

        # Layout
        self.rootWindow.columnconfigure(0, weight = 4)
        self.rootWindow.columnconfigure(1, weight = 6)
        self.rootWindow.rowconfigure(0, weight = 1)

        # style = ttk.Style(self.rootWindow)
        # style.configure(".",
        #     background = "#24b01c",
        #     font = ("System", 13)
        # )
        
    # -- Window methods --
    # Build the left pane: profile selection box
    # Might be obsolete since the data is (currently) unchanging
    def _buildProfileSelect(self):
        for x, button in enumerate(self.profileSelect.profbuttons):
            active = (self.pnum == x + 1)
            button["text"] = f"{'>' if active else ' '}  Profile {x + 1}  {'<' if active else ' '}"
            button.grid(row = x, sticky = "nsew")

    # Build the right pane: profile customization
    # We definitely need this as a function as the buttons will change for every different profile
    def _buildProfileView(self):
        for x, button in enumerate(self.profileView.keybuttons):
            bind = self.profile.keymap[Profile._keys[x]]
            btype = Bind.printTypeString(bind._type)
            code = bind.printDataStr()
            button["text"] = f"> {'0' if x < 9 else ''}{x + 1}        \n{btype}\n{code}"

    # Reprocesses the UI data so everything is up to date
    def _updateProfile(self, pnum):
        pnum, profile = change_profile(pnum)

        # change_profile will return pnum = 0 when the write fails
        if pnum < 0: return
        
        self.pnum = pnum
        self.profile = profile
        
        self._buildProfileSelect()      # Update profile which is shown as selected (left pane)
        self._buildProfileView()        # Update what the profile buttons say (right pane)

    # Handle the popup window for changing a key within the active profile
    def _modifyKey(self, key):
        if self.lock: return
        self.lock = True
        
        keyidx = Profile._keys[key]
        oldbind = self.profile.keymap[keyidx]
        newbind = Bind(oldbind._type, oldbind._data)

        bindWindow = tk.Toplevel(self.rootWindow)
        bindWindow.title(f"Edit Key {'0' if key < 9 else ''}{key + 1}")
        bindWindow.geometry("300x200")
        # bindWindow.style("KeyView.TFrame")

        bindWindow.columnconfigure(0, weight = 1)
        bindWindow.columnconfigure(1, weight = 1)
        bindWindow.rowconfigure(0, weight = 1)
        bindWindow.rowconfigure(1, weight = 1)
        
        typeLabel = tk.Label(bindWindow, text = "Key Type: ", font = ("System", 11))
        typeLabel.grid(row = 0, column = 0)

        typeListOpts = tk.Variable(value = (
            Bind.printTypeString(0),
            Bind.printTypeString(1),
            Bind.printTypeString(2),
            Bind.printTypeString(3),
            Bind.printTypeString(4)
        ))
        typeList = tk.Listbox(bindWindow, listvariable = typeListOpts, selectmode = tk.SINGLE, height = 5, font=("System", 11))
        typeList.grid(row = 0, column = 1, sticky = "ew")

        dataFrame = ttk.Frame(bindWindow)   # Debugging: style = "ProfileView.TFrame"
        dataFrame.columnconfigure(0, weight = 1)
        dataFrame.columnconfigure(1, weight = 1)
        dataFrame.grid(row = 1, column = 0, columnspan = 2, sticky = "nsew")

        # Inital processing of the UI
        # NOTE: This seems to work fine, though I fear we are not replacing this stringvar but instead 
        #       creating a new one for each key change, which may classify as a memory leak in practice.
        #       However, there exists no destroy member for a stringvar.
        typeList.selection_set(oldbind._type)
        dataValue = tk.StringVar()
        dataValue.set(oldbind._data)

        self._updateBindWindow(oldbind, newbind, typeList, dataFrame, dataValue, None)
        
        # Activate the event hooks
        typeList.bind("<<ListboxSelect>>", partial(self._updateBindWindow, oldbind, newbind, typeList, dataFrame, dataValue))
        bindWindow.bind("<Return>", lambda e: bindWindow.destroy())
        bindWindow.bind("<Escape>", partial(self._cancelKeyChange, bindWindow, oldbind, newbind))
        self.rootWindow.wait_window(bindWindow)

        # Window closed, attempt to change the keybind
        done = False
        newbind._data = dataValue.get()

        if not newbind.parseDataStr():
            print(f"Could not parse '{dataValue.get()}' as a {Bind.printTypeString(newbind._type)} bind. No changes have been made.")
            done = True
        elif oldbind == newbind: done = True
        elif newbind._type != 0 and newbind._data == 0:
            print("No data for keybind. No changes have been made.")
            done = True

        if not done:
            # Ensure key is within the expected bounds
            try: newkey = Profile._keys[key]
            except IndexError: newkey = 0
        
            # It is possible to press a key on the device and change the profile before writing changes
            # We mitigate this by changing to the expected profile though this does not guarantee success
            if newkey > 0:
                print(f"Editing key: {'0' if key < 9 else ''}{key + 1} 》{str(newbind)}")
                change_profile(self.pnum)
                modify_profile(newkey, newbind)
                self._updateProfile(self.pnum)      # Profile has changed but the UI won't until we call this

            else: print(f"Failed to edit key: Out of bounds")

        # Unlock the parent UI
        self.lock = False

    # Process bind window UI
    def _updateBindWindow(self, oldbind, newbind, listbox, frame, data, event):
        try: btype = listbox.curselection()[0]
        except IndexError: return

        # If this was called for an event we need to clean up the old entries
        if event is not None:
            # No need to change anything if the same entry was selected again
            if btype == newbind._type: return
            for child in frame.winfo_children():
                child.destroy()

        newbind._type = btype
        label = ttk.Label(frame, font = ("System", 11))
        display = None

        match btype:
            # KEY
            case 1:
                label["text"] = "Key (str/hex):"

                if btype == oldbind._type: data.set(oldbind.printDataStr())
                # elif event is None: data.set(Profile._codes[int(data.get())])    # This should never happen anymore
                else: data.set("")
                
                display = ttk.Entry(frame, textvariable = data, font = ("System", 11))

            # HYPERSHIFT
            case 2:
                label["text"] = "Profile:"

                if btype == oldbind._type: data.set(str(oldbind._data))
                elif event is not None: data.set("1")
                
                display = ttk.Spinbox(frame, textvariable = data, from_ = 1, to = MAXPROFILES_, font = ("System", 11))

            # PROFILE
            case 3:
                label["text"] = "Profile:"
                
                if btype == oldbind._type: data.set(str(oldbind._data))
                elif event is not None: data.set("1")
                
                display = ttk.Spinbox(frame, textvariable = data, from_ = 1, to = MAXPROFILES_, font = ("System", 11))

            # MACRO
            case 4:
                label["text"] = "Macro Key:"

                if btype == oldbind._type: data.set(oldbind.printDataStr())
                else: data.set("")
                
                display = ttk.Entry(frame, textvariable = data, font = ("System", 11))

        if display is None: return
        
        label.grid(row = 0, column = 0)
        display.grid(row = 0, column = 1, sticky = "ew")
        display.focus()
        display.icursor("end")

    def _cancelKeyChange(self, window, oldbind, newbind, event):
        newbind._type = oldbind._type
        newbind._data = oldbind._data
        window.destroy()

    def _saveActiveProfile(self):
        if self.lock: return

        # Lock the UI before opening the file dialogue
        self.lock = True

        # TODO: Default filename profile_{num}.rz
        filepath = tkfiledialog.asksaveasfilename(
            title = "Save profile...",
            initialfile = f"profile_{self.pnum}.rz",
            filetypes = [("Razer Profile", "*.rz")]
        )

        if len(filepath) > 0:
            print(f"Saving profile to '{filepath}'...")
            if save_profile(filepath): print("Done!")
            
        self.lock = False

    def _loadActiveProfile(self):
        if self.lock: return
        
        # Lock the UI before opening the file dialogue
        self.lock = True

        filepath = tkfiledialog.askopenfilename(
            title = "Load a profile...",
            filetypes = [("Razer Profile", "*.rz")]
        )

        if len(filepath) > 0:
            print(f"Loading profile '{filepath}'...")
            if load_profile(filepath):
                self._updateProfile(self.pnum)
                print("Done!")
        
        self.lock = False

    # -- Show the editor --
    def run(self):
        # Left Pane: profile selection
        self.profileSelect = Editor.ProfileSelect(self.rootWindow, self._updateProfile)
        self.profileSelect.grid(row = 0, column = 0, sticky = "nsew")

        # Right Pane: profile viewing/editing
        self.profileView = Editor.ProfileView(self.rootWindow, self._loadActiveProfile, self._saveActiveProfile, self._modifyKey)
        self.profileView.grid(row = 0, column = 1, sticky = "nsew")

        self._updateProfile(self.pnum)
        self.rootWindow.mainloop()

# Read a device file into a buffer
def read_device_file(devfile: str, ipath = None):
    if ipath is None: ipath = KBDPATH_
    path = DRIVERPATH_ + ipath + "/" + devfile
    buf = bytearray(4096)   # One page table in x86
    size = 0

    try:
        with io.open(path, "rb") as sysfile:
            size = sysfile.readinto(buf)

    except FileNotFoundError:
        print("Could not find device file. Is the the device plugged in?")

    return buf, size

def write_device_file(buf, devfile: str, ipath = None):
    if ipath is None: ipath = KBDPATH_
    path = DRIVERPATH_ + ipath + "/" + devfile
    size = 0

    try:
        with io.open(path, "wb") as sysfile:
            size = sysfile.write(buf)

    except FileNotFoundError:
        print("Could not find device file. Is the the device plugged in?")

    except PermissionError:
        print("Could not write to device file. Did you use sudo?")

    return size
    

# Swap to a profile
# Just return the active profile if !pnum
def change_profile(pnum: int = 0, show = False):
    if pnum == 0:
        buf, size = read_device_file("profile_num")
        if size > 0: pnum = int(buf.decode("utf-8").split("\n", 1)[0])
        else: pnum = -1
    else:
        buf = str(pnum).encode("utf-8")
        size = write_device_file(buf, "profile_num")   # returns number of bytes written
        if size == 0: pnum = -1
        
    profile = Profile()
    buf, size = read_device_file("profile")
    profile.load(buf, size)

    if show:
        print("> Active profile: " + str(pnum))
        print(profile)

    return pnum, profile

# Modify a keybind of the active profile
def modify_profile(key: int, bind: Bind):
    _, profile = change_profile(0)
    profile.keymap[key] = bind

    buf, _ = profile.store()
    write_device_file(buf, "profile")

    return profile

# Save a profile to disk
# Will use the active profile if none specified
def save_profile(path: str, profile = None):
    if profile is None: buf, _ = read_device_file("profile")
    else: buf, _ = profile.store()

    try:
        with io.open(path, "wb") as outfile:
            outfile.write(buf)

    except FileNotFoundError:
        print(f"Could not save profile. Is the path valid?")
        return False

    return True

# Load profile data from disk to the active profile
def load_profile(path: str):
    buf = bytearray(Profile._size)   # One page table in x86
    size = 0
    
    try:
        with io.open(path, "rb") as infile:
            size = infile.readinto(buf)

    except FileNotFoundError:
        print(f"Failed to load profile")
        return False

    size = write_device_file(buf, "profile")
    if size > 0: return True
    return False

if __name__ == "__main__":
    # Parse args (not technically consts, sorry if you're malding rn)
    EXEC = sys.argv[0]
    PROFILE_NUM = 0
    EDITOR = True
    SHOW = False
    DATA = None
    SAVE = None
    LOAD = None
    HELP = False

    arglist = sys.argv[1:]
    while len(arglist) > 0:
        arg = arglist.pop(0)
        # print(arg)

        # TODO: If the editor should have arguments, this will have to change
        if EDITOR: EDITOR = False

        # Many of the options should be made friendly with each other
        # EX: `linapse.py -m 5 1 34 -c 4` should swap to profile 4 and set key 05 to CTRL_KEY with data 34
        match arg:
            # Show the active profile (stdout)
            case "-v" | "--view" | "--show":
                SHOW = True
                
            # Swap device profile
            case "-c" | "--change" | "--swap":
                try: PROFILE_NUM = int(arglist.pop(0), 0)
                except ValueError: HELP = True

            # Edit a key in the active profile
            case "-m" | "--modify":
                key = 0
                btype = 0
                bdata = 0

                try:
                    key = arglist.pop(0)
                    btype = arglist.pop(0)
                    bdata = arglist.pop(0)

                except IndexError: HELP = True
                except ValueError: HELP = True
                
                DATA = (key, btype, bdata)

            # Save the active profile to disk
            case "-s" | "--save":
                SAVE = arglist.pop(0)

            # Load a profile from disk to the active profile
            case "-l" | "--load":
                LOAD = arglist.pop(0)

            # Unrecognized argument
            case _:
                HELP = True

        if HELP:
            print("Usage:")
            print("  > Show profile: -v [profile num] | --view, --show")
            print("  > Change profile: -c <profile num> | --change, --swap")
            print("  > Edit key: -m <key num> <type> <data> | --modify")
            print("  > Save profile: -s <path> | --save")
            print("  > Load profile: -l <path> | --load")
            # print("  > Help: -h, -?, --help, --usage")
            exit()

    # Determine keyboard interface device path
    DRIVERPATH_ = "/sys/bus/hid/drivers/hid-tartarus/"
    KBDPATH_ = None
    for name in os.listdir(DRIVERPATH_):
        path = DRIVERPATH_ + name
        if not os.path.islink(path): continue

        buf = bytearray(8)
        try:
            with io.open(path + "/intf_type", "rb") as sysfile:
                sysfile.readinto(buf)
        except FileNotFoundError: continue

        itype = buf.decode("utf-8").split("\n", 1)[0]
        if itype != "KBD": continue
        
        KBDPATH_ = name
        break;

    if KBDPATH_ is None:
        print("Could not find device files in /sys. Is the device plugged in?")
        exit(1)

    # Read max profile count
    buf, _ = read_device_file("profile_count")
    MAXPROFILES_ = int(buf.decode("utf-8").split("\n", 1)[0])

    # Run
    if PROFILE_NUM > 0:
        if PROFILE_NUM > MAXPROFILES_:
            # TODO: The user might want to cancel their operation if this happens
            print(f"Warning: Requested profile number exceeds {MAXPROFILES_}")
            
        print("Changing to profile:", PROFILE_NUM)
        change_profile(PROFILE_NUM)

    if LOAD:
        print(f"Loading profile '{LOAD}'")
        load_profile(LOAD)

    if SHOW: change_profile(0, True)

    if DATA:
        key = Profile._keys[int(DATA[0]) - 1]
        bind = Bind(DATA[1], DATA[2])
        if bind.parseDataStr():
            print(f"Editing key: {'0' if key < 10 else ''}{key} 》{str(bind)}")
            modify_profile(key, bind)
            
        else: print(f"Could not parse '{DATA[2]}' as a keybind. No changes have been made.")

    if SAVE:
        print(f"Saving profile '{SAVE}'")
        save_profile(SAVE)

    # --------
    if EDITOR:
        editor = Editor()
        editor.run()
