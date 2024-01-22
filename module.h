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

#define KEYLIST_LEN		8			// Maximum number of entries in the list of keys reported by the device
#define KEYMAP_LEN		26			// Number of unique keys supported by the device

#define MODKEY_MASK		0x80		// Applied to all modkey keycodes (i.e. 0000 0010 (lshift) -> 1000 0010)
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
	struct input_dev* input;	// Input device ref

	u8 profile;					// Current profile in use
	u8 inum;					// Interface number : 0 -> KB, 1 -> RGB???, 2 -> Mouse (wheel)
	void* idata;				// Interface data (currently unused)
};

// Driver data for keyboard interface
struct kbddata {
	// Stores the previous key state
	// u8 keylist[KEYLIST_LEN];
	u8 modkey;
	union keystate {
		u8 b[32];
		u32 comp[8];
	} state;
};

/*/ OLD INTERFACE DATA STRUCTS
// Private data for keyboard interface (inum 0x00)
//	^^To be stored at drvdata.data
struct keyboard_data {
	// For parsing events
	int keycount;
	u8 keylist[KEYLIST_LEN];
	u8 modkey;

	// Input handling data
	char hypershift;

	// Device profile
	struct bind* map;
	struct bind* map_hs;

	// TODO PROFILES
	// For implementation:
	// Device data contains array of keymap pointers (size 2 * 8?)
	// Each pair denotes normal/hs for a given profile
	// A profile swap action replaces map and map_hs with the respective profiles
	// Consider: Reset hypershift state to 0 on profile swap

	// TODO MACROS
	// For implementation:
	// Similar to profiles, use array of pointers
};

// Private sector for mouse HID (inum 0x02)
struct mouse_data {
	int debug;		// Just a value so there's something there
};  //*/

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


// HANLDERS (device event hooks)
static int device_probe(struct hid_device*, const struct hid_device_id*);
static int input_config (struct hid_device*, struct hid_input*);
static void device_disconnect(struct hid_device*);
static int handle_event (struct hid_device*, struct hid_report*, u8*, int);
static int mapping_bypass (struct hid_device* hdev, struct hid_input* hidinput, struct hid_field* field,
			struct hid_usage* usage, unsigned long** bit, int* max) { return -1; }


// INPUT PROCESSING
void log_event (u8*, int, u8);
struct bind key_event (struct kbddata*, int*, u8*, int);
// TODO: Determine if function is needed to handle out-of-order errors


// DEVICE COMMANDS
void log_report(struct razer_report* report);
unsigned char report_checksum(struct razer_report* report);
struct razer_report init_report(unsigned char class, unsigned char id, unsigned char size);
static struct razer_report send_command(struct hid_device* dev, struct razer_report* req, int* cmd_errno);


// MODULE
MODULE_AUTHOR("Drayux");
MODULE_DESCRIPTION("Tartarus V2 Driver");
MODULE_LICENSE("GPL");

static struct hid_device_id id_table [] = {
	{ HID_USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ 0 }
}; 

MODULE_DEVICE_TABLE(hid, id_table);

static struct hid_driver tartarus_driver = {
	.name = "hid-tartarus_v2",
	.id_table = id_table,
	.input_configured = input_config,
	.probe = device_probe,
	.remove = device_disconnect,
	.raw_event = handle_event,
	.input_mapping = mapping_bypass
}; 

// Initalize the module with the kernel
module_hid_driver(tartarus_driver);
