#ifdef _TARTARUS_HID
// Some ✨quirky✨ things with my current module design
// AKA This entire header should be refactored if we're going to need multiple modules to depend upon it
#error module.h included in multiple files
#endif
#define _TARTARUS_HID

#include <linux/hid.h>
#include <linux/kernel.h>
#include <linux/module.h>

// PROPERTIES
#define VENDOR_ID		0x1532		// Razer USA, Ltd
#define PRODUCT_ID		0x022b		// Tartarus_V2

#define REPORT_LEN  	0x5A		// Each USB report has 90 bytes
#define REPORT_IDX  	0x01		// Report/Response (consistent across all devices so I don't think I need two defs)
#define WAIT_MIN    	600         // Minmum response wait time is 600 microseconds (0.6 ms)
#define WAIT_MAX    	800         // ^^Maximum is 800 us (0.8 ms)

#define KBD_INUM		0x00		// Interface number of the keyboard is 0
#define EXT_INUM		0x01		// Unknown interface (keyboard?)
#define MOUSE_INUM		0x02 		// Interface number of the mouse (wheel) is 2

#define KEYLIST_LEN		8			// Maximum number of entries in the list of keys reported by the device
#define KEYMAP_LEN		26			// Number of unique keys supported by the device
#define PROFILE_COUNT	8			// Number of profiles stored in the driver

#define MODKEY_MASK		0x40		// Applied to all modkey keycodes (i.e. 0000 0010 (lshift) -> 0100 0010)
#define MODKEY_SHIFT	0x02		// Bit pattern for the shift key (key 16)
#define MODKEY_ALT		0x04		// Bit pattern for the alt key (circular thumb button)


// COMMANDS
#define CMD_KBD_LAYOUT  0x00, 0x86, 0x02	// Query the device for its keyboard layout
#define CMD_SET_LED     0x03, 0x00, 0x03	// Set a specified LED with a given value


// KEYBINDING
#define CTRL_NOP		0x00		// No key action
#define CTRL_KEY     	0x01		// Keyboard button action
#define CTRL_SHIFT      0x02		// Hypershift mode action	(swap profile while held)
#define CTRL_PROFILE   	0x03		// Change profile action	(swap profile upon press)
#define CTRL_MACRO		0x04		// Play macro action 		(playback list of key actions)
#define CTRL_MMOV		0x05		// Move the mouse
#define CTRL_MWHEEL		0x06		// Mouse wheel action


// STRUCTS
// Defines the behavior of a key
struct bind {
	u8 type;		// Event type
	u8 data;		// Respective data (key code or index of macro)
};

// Device driver data (for passing data across functions; unique per interface)
struct drvdata {
	u8 profile;					// Active profile number (keyboard and mouse have one each)
	u8 inum;					// Interface number : 0 -> KB, 1 -> RGB???, 2 -> Mouse (wheel)
	void* idata;				// Interface data (keyboard, mouse, etc.)
	struct input_dev* input;	// Input device ref (for sending inputs to kernel)
	struct mutex lock;
};

// Driver data for keyboard interface
struct kbddata {
	// Stores the previous key state
	// u8 keylist[KEYLIST_LEN];
	u8 modkey;
	union keystate {
		u8 bytes[32];
		u32 words[8];
	} state;

	// Profile handling
	u8 shift;			// Index of "active" profile key (usually hypershift button) ; Used for profiles so the "release" of the key can be ignored ; 0 if nothing to do
	u8 prev_profile;	// ID of profile to return to upon release ; 0(?) if profile should be kept (profile swap)

	// Array of keymap arrays, corresponding to the device profiles
	// Profiles are identified with values 1-8, corresponding to indexes 0-7 here
	struct profile {
		struct bind keymap[0x100];
	} maps[PROFILE_COUNT];
};

struct mousedata {
	struct mprofile {
		struct bind keymap[8];
	} maps[PROFILE_COUNT];
};

// TODO: Wild idea for a "mouse" bind
// 		 Middle click on the scroll wheel goes to "profile mode"
//		 Rolling the wheel up or down (in this mode) will "roll" through the profiles
//		 EX: I might have a couple different profiles for convenient binds for portal 2 speedruns

// Format of the 90 byte device response
// (Taken from OpenRazer driver)
struct razer_report {
	// Response status
	//  0x00 - New Command
	//  0x01 - Device Busy
	//  0x02 - Success
	//  0x03 - Failure
	//  0x04 - Timeout
	//  0x05 - Not Supported
	unsigned char status;

	// Transaction ID (transaction_id)
	// Allows for communicating with multiple devices on one USB
	// (Still haven't figured out how the interface works yet--probably not needed)
	union {
		unsigned char id;
		struct transaction_parts {
			unsigned char device : 3;
			unsigned char id : 5;
		} parts;
	} tr_id;

	// Remaining packets (remaining_packets)
	// Big endian from device
	// (Also not sure yet how this is used/what exactly it means in this scope)
	unsigned short remaining;

	// Protocol Type (protocol_type)
	// Only provided comment is 'always 0x0'
	// Probably deprecated Razer feature
	unsigned char type;

	// Size of the payload (data_size)
	// Maximum is 80 bytes as the header and 'footer' components consume 10
	unsigned char size;

	// Command type (command_class)
	// Use this to specify what the device should do for us (basically our call parameter)
	// Device should always return with the same class as the request
	// ????   Actually this could be command_id - Needs more digging   ?????
	unsigned char class;

	// Command ID (command_id)
	// The exact command we are using
	// Direction [0] is   Host -> Device
	//           [1] is Device -> Host
	union _command_id {
		unsigned char id;
		struct command_id_parts {
			unsigned char direction : 1;
			unsigned char id : 7;
		} parts;
	} cmd_id;

	// Command data
	// If [Host -> Device] : This is where we place command arguments
	// If [Device -> Host] : This contains the device response data
	unsigned char data[80];

	// Argument/Data checksum (crc)
	// Algorithm XORs each byte of the report, starting at 2 and ending at 87 (inclusive) [Skips bytes 0, 1, 88, 89]
	unsigned char cksum;

	// Unused, always 0x0
	unsigned char reserved;
};

/*/ TODO: Sysfs format
// The following currently exists exclusively for reference
struct config {
	u8 version_major;		// First half of the config version
	u8 version_minor;		// Second half of the config version
	u8 num_profiles;		// How many profiles exist in the config
	u8 num_macros;			// How many macros are defined in the config*
	u32 unused;

	// Array of profile types
	struct profile {
		u8 led;				// Profile indicator light(s) (TODO: Consider automating this based upon profile number)
		struct bind keymap[0xFF];
	}* profiles;
	// struct bind (* keymap)[0xFF];	// NOTE: Syntax equivalent to the above omitting the led field

	// struct macro* macros;			// Macros are of variable length (TODO: Consider moving them to seperate file)
}; //*/


// HANLDERS (device event hooks)
static int device_probe(struct hid_device*, const struct hid_device_id*);
static int input_config (struct hid_device*, struct hid_input*);
static void device_disconnect(struct hid_device*);
static int handle_event (struct hid_device*, struct hid_report*, u8*, int);
static int mapping_bypass (struct hid_device* hdev, struct hid_input* hidinput, struct hid_field* field,
			struct hid_usage* usage, unsigned long** bit, int* max) { return -1; }

void set_profile_num(struct device*, u8);
static ssize_t profile_num_show(struct device*, struct device_attribute*, char*);
static ssize_t profile_num_store(struct device*, struct device_attribute*, const char*, size_t);

static ssize_t profile_show(struct device*, struct device_attribute*, char*);
static ssize_t profile_store(struct device*, struct device_attribute*, const char*, size_t);


// INPUT PROCESSING
void log_event (u8*, int, u8);
struct bind key_event (struct kbddata*, u8, int*, u8*, int);
// TODO: Determine if out-of-order errors exist (and need to be handled)


// DEVICE COMMANDS
void log_report(struct razer_report*);
unsigned char report_checksum(struct razer_report*);
struct razer_report init_report(unsigned char, unsigned char, unsigned char);
struct razer_report send_command(struct device*, struct razer_report*, int*);


// DEVICE ATTRIBUTES (connects functions to udev events)
static DEVICE_ATTR(profile_num, 0644, profile_num_show, profile_num_store);		// static DEVICE_ATTR_RW(profile_num);
static DEVICE_ATTR(profile, 0644, profile_show, profile_store);


// MODULE
MODULE_AUTHOR("Drayux");
MODULE_DESCRIPTION("Driver for Razer Tartarus.v2");
MODULE_LICENSE("GPL");

static struct hid_device_id id_table [] = {
	{ HID_USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ 0 }
};

MODULE_DEVICE_TABLE(hid, id_table);

static struct hid_driver hid_tartarus = {
	.name = "hid-tartarus",
	.id_table = id_table,
	.input_configured = input_config,
	.probe = device_probe,
	.remove = device_disconnect,
	.raw_event = handle_event,
	.input_mapping = mapping_bypass
};

// Initalize the module with the kernel
module_hid_driver(hid_tartarus);
