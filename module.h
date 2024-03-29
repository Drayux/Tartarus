#ifdef _TARTARUS_HID
// Some ✨quirky✨ things with my current module design
// AKA This entire header should be refactored if we're going to need multiple modules to depend upon it
#error module.h included in multiple files
#endif
#define _TARTARUS_HID

#include <linux/hid.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/usb.h>

// PROPERTIES
#define VENDOR_ID		0x1532		// Razer USA, Ltd
#define PRODUCT_ID		0x022b		// Tartarus_V2

#define PROFILE_COUNT	8			// Number of profiles stored in the driver (each profile ~0.5 KB)
#define KEYLIST_LEN		8			// Maximum number device-supported simultaneous keypresses (6 normal keys + shift and alt)
#define KEYMAP_LEN		0x100		// Number of entries in a complete keymap
#define REPORT_LEN  	0x5A		// Size of a USB control report (90 bytes)

#define KBD_INUM		0x00		// Interface number of the keyboard is 0
#define EXT_INUM		0x01		// Unknown interface (keyboard?)
#define MOUSE_INUM		0x02 		// Interface number of the mouse (wheel) is 2


// COMMANDS (TODO: Consider removing old URB functions, making these obsolete)
#define CMD_KBD_LAYOUT  0x00, 0x86, 0x02	// Query the device for its keyboard layout
#define CMD_SET_LED     0x03, 0x00, 0x03	// Set a specified LED with a given value


// KEYS
#define RZKEY_01		0x1E		// Funny names because we'd conflict with existing defines in linux kernel
#define RZKEY_02		0x1F
#define RZKEY_03		0x20
#define RZKEY_04		0x21
#define RZKEY_05		0X22
#define RZKEY_06		0x2B
#define RZKEY_07		0x14
#define RZKEY_08		0x1A
#define RZKEY_09		0x08
#define RZKEY_10		0x15
#define RZKEY_11		0x39
#define RZKEY_12		0x04
#define RZKEY_13		0x16
#define RZKEY_14		0x07
#define RZKEY_15		0x09
#define RZKEY_16		0x42		// (Actual 0x02 -> Shift)
#define RZKEY_17		0x1D
#define RZKEY_18		0x1B
#define RZKEY_19		0x06
#define RZKEY_20		0x2C
#define RZKEY_CIRCLE	0x44		// (Actual 0x04 -> Alt)
#define RZKEY_THMB_L	0x50
#define RZKEY_THMB_U	0x52
#define RZKEY_THMB_R	0x4F
#define RZKEY_THMB_D	0x51

#define MODKEY_SHIFT	0x02		// Bit pattern for the shift key (key 16)
#define MODKEY_ALT		0x04		// Bit pattern for the alt key (circular thumb button)
#define MODKEY_MASK		0x40		// Applied to all modkey keycodes (i.e. 0000 0010 (lshift) -> 0100 0010)
#define MWHEEL_BTN		0x04		// Bit pattern for the mouse wheel button
#define MWHEEL_WHEEL	0x08

// BINDS
#define CTRL_NOP		0x00		// No key action
#define CTRL_KEY     	0x01		// Keyboard button
#define CTRL_SHIFT      0x02		// Hypershift mode	(swap profile while held)
#define CTRL_PROFILE   	0x03		// Change profile	(swap profile upon press)
#define CTRL_MACRO		0x04		// TODO: Play macro action 		(playback list of key actions)
#define CTRL_SCRIPT		0x05		// TODO: Execute script relative to the current user's home dir
#define CTRL_SWKEY		0x06		// TODO: Key that will be "swapped" upon hypershift state change
#define CTRL_MMOV		0x07		// TODO: Move the mouse
#define CTRL_MWHEEL		0x08		// TODO: Mouse wheel action
#define CTRL_DEBUG		0xFF		// (DEBUG)


// STRUCTS
// Defines the behavior of a key
struct bind {
	u8 type;		// Event type
	u8 data;		// Respective data (key code or index of macro)
};

// Single event and its respective state
struct event {
	u8 idx;			// "Key" index
	u8 state;		// 0: Release ; 1: Press
};

// Bitmap container for interface key states
struct keystate {
	union {
		u8 bytes [32];
		u32 data [8];
	};
};

// Device driver data (for passing data across functions; unique per interface)
struct drvdata {
	u8 profile;					// Active profile number (keyboard and mouse have one each)
	u8 inum;					// Interface number : 0 -> KB, 1 -> RGB???, 2 -> Mouse (wheel)
	void* idata;				// Interface data (keyboard, mouse, etc.)
	struct usb_device* parent;	// Parent device ref (for sending URBs)
	struct input_dev* input;	// Input device ref (for sending inputs to kernel)
	struct mutex lock;
};

// Driver data for keyboard interface
struct kbddata {
	u8 keylist [KEYLIST_LEN];			// Device button state
	struct keystate shift_keylist;		// Keys pressed within hypershift mode
	struct keystate ignore_keylist;		// Keys where their release should be ignored
	
	u8 shift;							// Current hypershift profile number
	u8 revert;							// Hypershift return profile number ; 0 -> NOP
	
	// Device profiles numbers range 1-8, corresponding to indexes 0-7 ; 0 -> Device disabled
	struct profile {
		struct bind keymap [KEYMAP_LEN];
	} maps [PROFILE_COUNT];
};

struct mousedata {
	// u8 mwheel_state;					// Value directly from raw event (not clamped)
	struct mprofile {
		struct bind keymap [8];
	} maps [PROFILE_COUNT];
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
	unsigned char data [80];

	// Argument/Data checksum (crc)
	// Algorithm XORs each byte of the report, starting at 2 and ending at 87 (inclusive) [Skips bytes 0, 1, 88, 89]
	unsigned char cksum;

	// Unused, always 0x0
	unsigned char reserved;
};

// Buffers for asynchronous control URBs
struct urb_context {
	struct usb_ctrlrequest setup;
	struct razer_report req;
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
static int device_probe (struct hid_device*, const struct hid_device_id*);
static int input_config (struct hid_device*, struct hid_input*);
static void device_disconnect (struct hid_device*);
static int handle_event (struct hid_device*, struct hid_report*, u8*, int);
static int mapping_bypass (struct hid_device* hdev, struct hid_input* hidinput, struct hid_field* field,
			struct hid_usage* usage, unsigned long** bit, int* max) { return -1; }

static ssize_t intf_type (struct device*, struct device_attribute*, char*);
static ssize_t profile_count (struct device*, struct device_attribute*, char*);

static ssize_t profile_num_show (struct device*, struct device_attribute*, char*);
static ssize_t profile_num_store (struct device*, struct device_attribute*, const char*, size_t);

static ssize_t profile_show (struct device*, struct device_attribute*, char*);
static ssize_t profile_store (struct device*, struct device_attribute*, const char*, size_t);


// INPUT PROCESSING
void log_event (u8*, int, u8);
int process_event_kbd (struct event*, u8*, u8*, int);
void resolve_event_kbd (struct event*, struct drvdata*);
u8 lookup_profile_kbd (struct kbddata*, struct bind*, u8, u8, u8);
void swap_profile_kbd (struct drvdata*, u8, struct keystate*);
void set_profile (struct drvdata*, u8);
int process_event_mouse (struct event*, u8*, int);
void resolve_event_mouse (struct event*, struct drvdata*);
// void swap_profile_mouse ( ... );

// void swap_profile_kbd_old (struct device*, struct drvdata*, u8);

// DEVICE COMMANDS
void log_report (struct razer_report*);
unsigned char report_checksum (struct razer_report*);
struct razer_report init_report (unsigned char, unsigned char, unsigned char);
struct razer_report send_command (struct device*, struct razer_report*, int*);
void set_profile_led (struct drvdata*, u8, u8);
void set_profile_led_complete (struct urb*);


// DEVICE ATTRIBUTES (connects functions to udev events)
static DEVICE_ATTR(intf_type, 0444, intf_type, NULL);
static DEVICE_ATTR(profile_count, 0444, profile_count, NULL);
static DEVICE_ATTR(profile_num, 0644, profile_num_show, profile_num_store);		// static DEVICE_ATTR_RW(profile_num);
static DEVICE_ATTR(profile, 0644, profile_show, profile_store);


// MODULE
MODULE_AUTHOR("Drayux");
MODULE_DESCRIPTION("Some synapse features for the Razer Tartarus v2");
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
