import io
import sys
import tkinter as tk

class Bind:
    def __init__(self, _type, _data):
        self._type = _type  #ord(_type)
        self._data = _data  #ord(_data)

    def __str__(self):
        ret = ""
        match self._type:
            case 0: ret += "NONE"
            case 1: ret += "KEY"
            case 2: ret += "SHIFT"
            case 3: ret += "PROFILE"
            case 4: ret += "MACRO"
            case 5: ret += "SCRIPT"
            case 6: ret += "SWKEY"
            case 255: ret += "DEBUG"
            case other: ret += "UNDEFINED"

        ret += " : "
        ret += hex(self._data)

        return ret

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
        "",     "ESC",  "1",    "2",    "3",    "4",    "5",    "6",    "7",    "8",        #   0 -   9
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
        "SCALE","KP,",  "HANGE","HANJA","YEN",  "LMETA","RMETA","COMP"
    ]

    # TODO: This could use bounds checking
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

                ret += "┃ "
                match btype:
                    case 0: ret += "NONE"
                    case 1: ret += "KEY"
                    case 2: ret += "SHIFT"
                    case 3: ret += "PROFL"
                    case 4: ret += "MACRO"
                    case 5: ret += "SCRIPT"
                    case 6: ret += "SWKEY"
                    case 255: ret += "DEBUG"
                    case other: ret += "UNDEF"
                ret += "\t┃\t"

            # Row 3 -> bind data
            ret += "\n"
            for j in range(5):
                key = Profile._keys[key_idx + j]
                btype = self.keymap[key]._type
                bdata = self.keymap[key]._data

                if (btype == 1): ret += f"┃ {Profile._codes[bdata]}\t┃\t"
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
    def __init__(self):
        self.activeProfile = 0
        
        self.rootWindow = tk.Tk()
        self.rootWindow.title("Linapse Configurator")

    # -- Window methods --
    def _buildKey(self):
        pass

    # TODO: Construct a special shape for the thumb pseudo joystick
    def _buildThumbKey(self):
        pass
        
    def _buildProfileView(self):
        self.debug = tk.Label(self.rootWindow, text = "Debug message :)")

    # Program args will essentially be used here
    def run(self):
        self._buildProfileView()

        # self.rootWindow.pack()
        self.rootWindow.mainloop()

# TODO: needs detection for which interface is which (handle this in the parse args step)
# Read a device file into a buffer
def read_device_file(devfile: str, ipath = "0003:1532:022B.0001"):
    path = "/sys/bus/hid/drivers/hid-tartarus/" + ipath + "/" + devfile
    buf = bytearray(4096)   # One page table in x86
    size = 0

    try:
        with io.open(path, "rb") as sysfile:
            size = sysfile.readinto(buf)

    except FileNotFoundError:
        print("Could not find device file. Is the the device plugged in?")

    return buf, size

def write_device_file(buf, devfile: str, ipath = "0003:1532:022B.0001"):
    path = "/sys/bus/hid/drivers/hid-tartarus/" + ipath + "/" + devfile
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
        buf, _ = read_device_file("profile_num")
        pnum = buf.decode("utf-8").split("\n", 1)[0]
    else:
        buf = str(pnum).encode("utf-8")
        write_device_file(buf, "profile_num")   # returns number of bytes written
        
    profile = Profile()
    buf, size = read_device_file("profile")
    profile.load(buf, size)

    if show:
        print("> Active profile: " + pnum)
        print(profile)

    return profile

# Modify a keybind of the active profile
def modify_profile(key: int, bind: Bind):
    profile = change_profile(0)
    profile.keymap[key] = bind

    buf, _ = profile.store()
    write_device_file(buf, "profile")

    return profile

# Save a profile to disk
# Will use the active profile if none specified
def save_profile(path: str, profile = None):
    # TODO: Get the current profile number so we swap back to it when we're done (nevermind do this in the editor save function)
    if profile is None: buf, _ = read_device_file("profile")
    else: buf, _ = profile.store()

    try:
        with io.open(path, "wb") as outfile:
            outfile.write(buf)

    except FileNotFoundError:
        print(f"Could not save profile. Does the directory exist?")

# Load profile data from disk to the active profile
def load_profile(path: str):
    buf = bytearray(Profile._size)   # One page table in x86
    size = 0
    
    try:
        with io.open(path, "rb") as infile:
            size = infile.readinto(buf)

    except FileNotFoundError:
        print(f"Failed to load profile")

    write_device_file(buf, "profile")

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
                    btype = int(arglist.pop(0), 0)
                    bdata = int(arglist.pop(0), 0)

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

    # Run
    if PROFILE_NUM > 0:
        print("Changing to profile:", PROFILE_NUM)
        change_profile(PROFILE_NUM)

    if LOAD:
        print(f"Loading profile '{LOAD}'")
        load_profile(LOAD)

    if SHOW: change_profile(0, True)

    if DATA:
        # TODO: Parse the bind type and value from Profile._codes
        key = Profile._keys[int(DATA[0]) - 1]
        bind = Bind(DATA[1], DATA[2])
        print(f"Editing key: {key} 》{str(bind)}")
        modify_profile(key, bind)

    if SAVE:
        print(f"Saving profile '{SAVE}'")
        save_profile(SAVE)

    # --------
    if EDITOR:
        editor = Editor()
        editor.run()
