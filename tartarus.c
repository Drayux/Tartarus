#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>      // This import might not be needed
#include <linux/hid.h>
#include <linux/string.h>   // For use with memset and memcpy

#include "default_map.h"

// -- DRIVER METADATA --
MODULE_AUTHOR("Drayux");
MODULE_DESCRIPTION("Tartarus V2 Driver");
MODULE_LICENSE("GPL");

// -- DEVICE SUPPORT --
#define VENDOR_ID		0x1532		// Razer USA, Ltd
#define PRODUCT_ID		0x022b		// Tartarus_V2

// -- DEVICE INTERFACING --
#define REPORT_LEN  	0x5A		// Each USB report has 90 bytes
#define REPORT_IDX  	0x01		// Report/Response (consistent across all devices so I don't think I need two defs)
#define WAIT_MIN    	600         // Minmum response wait time is 600 microseconds (0.6 ms)
#define WAIT_MAX    	800         // ^^Maximum is 800 us (0.8 ms)

#define KEYLIST_LEN		8			// Maximum number of entries in the list of keys reported by the device
#define	KEYMAP_LEN		26			// Number of unique keys supported by the device

#define MODKEY_MASK		0x80		// Applied to all modkey keycodes (i.e. 0000 0010 (lshift) -> 1000 0010)
#define MODKEY_SHIFT	0x02		// Bit pattern for the shift key (key 16)
#define MODKEY_ALT		0x04		// Bit pattern for the alt key (circular thumb button)

// Mapping events (u8 type)
#define CTRL_NOP		0x00		// No key action
#define CTRL_KEYMAP 	0x01		// Keyboard button action
#define CTRL_HYPERSHIFT 0x02		// Hypershift mode action	(TODO hold/toggle options)
#define CTRL_MACRO		0x03		// Play macro action 		(TODO play while held/toggle options)
#define CTRL_PROFILE	0x04		// Change profile action	(TODO any of this part--gonna save it for last because I don't use it)

// Commands
#define CMD_KBD_LAYOUT  0x00, 0x86, 0x02	// Query the device for its keyboard layout
#define CMD_SET_LED     0x03, 0x00, 0x03	// Set a specified LED with a given value

// Event hooks
static int input_config (struct hid_device*, struct hid_input*);
static int device_probe(struct hid_device*, const struct hid_device_id*);
static void device_disconnect(struct hid_device*);
static int event_handler (struct hid_device*, struct hid_report*, u8*, int);
static int bypass_mapping (struct hid_device *hdev,
			struct hid_input *hidinput, struct hid_field *field,
			struct hid_usage *usage, unsigned long **bit, int *max) { return -1; }

// Array of USB device ID structs
static struct hid_device_id id_table [] = {
	{ HID_USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ 0 }
}; MODULE_DEVICE_TABLE(hid, id_table);

// Driver metadata struct
static struct hid_driver tartarus_driver = {
	.name = "razer_tartarus_usb",
	.id_table = id_table,
	.input_configured = input_config,
	.probe = device_probe,
	.remove = device_disconnect,
	.raw_event = event_handler,
	.input_mapping = bypass_mapping
};

module_hid_driver(tartarus_driver);


// -- STRUCTS --
// STRUCT KEYMAP CURRENTLY INSIDE default_map.h
// Move it back when profiles are working!
struct keymap;

// Private sector for keyboard HID (inum 0x00)
// TODO DEVICE PROFILES (probably going to reset keymap/keymap_hypershift with a load function?)
struct keyboard_data {
	// Input device
	struct input_dev* input;

	// For parsing events
	int keycount;
	u8 keylist[KEYLIST_LEN];
	u8 modkey;

	// Input handling data
	char hypershift;

	// Device profile
	// struct keymap map[KEYMAP_LEN];
	// struct keymap map_hs[KEYMAP_LEN];

	struct keymap* map;
	struct keymap* map_hs;

	// TODO array of macros
};

// TODO private sector for mouse device

// Format of the 90 byte device response
// (Taken from OpenRazer)
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
	union _transaction_id {
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

// Log the a razer report struct to the kernel (currently used for debugging)
void log_report (struct razer_report* report) {
	char* status_str;
	char* params_str;
	int i;      // Top-level loop parameter

	// Status
	switch (report->status) {
	case 0x00:
		status_str = "New Command (0x00)";
		break;
	case 0x01:
		status_str = "Device Busy (0x01)";
		break;
	case 0x02:
		status_str = "Success (0x02)";
		break;
	case 0x03:
		status_str = "Failure (0x03)";
		break;
	case 0x04:
		status_str = "Timeout (0x04)";
		break;
	case 0x05:
		status_str = "Not Supported (0x05)";
		break;
	default:
		status_str = "WARNING unexpected status";
	};
	
	// Command params (data)
	// Length of param string: 80 * 2 chars, + (80 / 2) * 2 spaces + 1 null = 241
	params_str = (char*) kzalloc(250 * sizeof(char), GFP_KERNEL);
	for (i = 0; i < 80; i += 2)
		snprintf(params_str + (3 * i), 7, "%02x%02x%s", report->data[i], report->data[i + 1], ((i + 2) % 16) ? "  " : "\n\t");

	printk(KERN_INFO "TARTARUS DEBUG INFORMATION:\n\n\tTransaction ID: 0x%02x\tStatus: %s\n\tCommand ID: 0x%02x\tClass: 0x%02x\n\tSize: 0x%02x (%d)\t\tRemaining Packets: 0x%02x (%d)\n\n\tData: 0x\n\t%s\n",
		   report->tr_id.id,
		   status_str,
		   report->size, report->size,
		   report->remaining, report->remaining,
		   report->class,
		   report->cmd_id.id,
		   params_str);

	kfree(params_str);
}

// Create a razer report data type
// Specifically for use in requesting data from the device
// (Core operation taken from OpenRazer)
struct razer_report generate_report (unsigned char class, unsigned char id, unsigned char size) {
	struct razer_report report = { 0 };
	// Static values
	report.tr_id.id = 0xFF;
	// report.status = 0x00;
	// report.remaining = 0x00;
	// report.protocol_type = 0x00;

	// Command parameters
	report.class = class;
	report.cmd_id.id = id;
	report.size = size;

	return report;
}


// -- DEVICE INTERFACING --
// Calculate report checksum
// (From OpenRazer)
unsigned char razer_checksum (struct razer_report* report) {
	unsigned char ck = 0;
	unsigned char* bytes = (unsigned char*) report;

	for (int i = 2; i < 88; i++) ck ^= bytes[i];
	return ck;
}

// Send prepared device URB
// Returns the device response
// Use req (aka the request report) to specify the command to send
// cmd_errno can be NULL (but shouldn't be)
// (Logic from OpenRazer)
static struct razer_report send_command (struct hid_device* dev, struct razer_report* req, int* cmd_errno) {
	char* request;      // razer_report containing the command parameters (i.e. get layout/set lighting pattern/etc)
	int received = -1;  // Amount of data transferred (result of usb_control_msg)

	struct usb_interface* parent = to_usb_interface(dev->dev.parent);
	struct usb_device* tartarus = interface_to_usbdev(parent);

	// Function output
	// Allocated on the stack and copied at return so no memory cleanup needed
	struct razer_report response = { 0 };

	// Allocate necessary memory and check for errors
	request = (char*) kzalloc(sizeof(struct razer_report), GFP_KERNEL);
	if (!request) {         // (!(request && response))
		printk(KERN_WARNING "Failed to communcate with device: Out of memory.\n");
		if (cmd_errno) *cmd_errno = -ENOMEM;
		return response;
	}

	// Copy data from our request struct to the fresh pointer
	req->cksum = razer_checksum(req);
	memcpy(request, req, sizeof(struct razer_report));

	// Step one - Attempt to send the control report
	// This sends the data to the device and sets the internal "respond to me" bit
	//  0x09 --> HID_REQ_SET_REPORT
	//  0X21 --> USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT
	// (Looks like this can also be done with usb_fill_control_urb followed by usb_submit_urb functions)
	received = usb_control_msg(tartarus, usb_sndctrlpipe(tartarus, 0), 0x09, 0X21, 0x300, REPORT_IDX, request, REPORT_LEN, USB_CTRL_SET_TIMEOUT);
	usleep_range(WAIT_MIN, WAIT_MAX);
	if (received != REPORT_LEN) {
		printk(KERN_WARNING "Device data transfer failed.\n");
		if (cmd_errno) *cmd_errno = (received < 0) ? received : -EIO;
		goto send_command_exit;
	}

	// Step two - Attempt to get the data out
	// We've prepared an empty buffer, and the device will populate it
	//  0x01 --> HID_REQ_GET_REPORT
	//  0XA1 --> USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN0x00, 0x86, 0x02
	memset(request, 0, sizeof(struct razer_report));
	received = usb_control_msg(tartarus, usb_rcvctrlpipe(tartarus, 0), 0x01, 0XA1, 0x300, REPORT_IDX, request, REPORT_LEN, USB_CTRL_SET_TIMEOUT);
	if (received != REPORT_LEN) {
		// An error here will likely be a mismatched data len
		// We will still return, but should warn that the data might be invalid
		printk(KERN_WARNING "Invalid device data transfer. (%d bytes != %d bytes)\n", received, REPORT_LEN);
		if (cmd_errno) *cmd_errno = received;
	}

	// We aren't referencing the buffer so we copy it to the stack (rename if response var is used)
	memcpy(&response, request, sizeof(struct razer_report));

	send_command_exit:
	// Final cleanup
	kfree(request);
	return response;
}


// DEVICE INPUT CONFIGURATION
// ~Called before probe~
// Specify device input parameters (input_dev event types, available keys, etc.)
static int input_config (struct hid_device* dev, struct hid_input* input) {
	struct keyboard_data* device_data = NULL;
	struct input_dev* input_dev = input->input;

	// Prepare our data for the private sector
	device_data = kzalloc(sizeof(struct keyboard_data), GFP_KERNEL);
	if (!device_data) return -ENOMEM;
	hid_set_drvdata(dev, device_data);

	device_data->input = input_dev;		// Thanks https://github.com/nirenjan/libx52/blob/482c5980abd865106a418853783692346e19ebb6/kernel_module/hid-saitek-x52.c#L124
	// device_data->keycount = 0;
	// device_data->hypershift = 0;
	device_data->map = default_map;		// DEBUG (pending completion of profiles)

	// Specify our input conditions
	set_bit(EV_KEY, input_dev->evbit);
	for (int i = 0; i < 25; i++) set_bit(KEY_MACRO1 + i, input_dev->keybit);

	set_bit(KEY_A, input_dev->evbit);

	return 0;
}

// DEVICE PROBE
// Set up private device data
// Debug: Send sample URB for keyboard layout
static int device_probe (struct hid_device* dev, const struct hid_device_id* id) {
	int status;
	struct keyboard_data* device_data = hid_get_drvdata(dev);

	struct razer_report cmd;
	struct razer_report out;

	struct usb_interface* interface = to_usb_interface(dev->dev.parent);
	struct usb_device* device = interface_to_usbdev(interface);

	u8 inum = interface->cur_altsetting->desc.bInterfaceNumber;

	// Attempt to start communication with the device
	// (I think this populates some dev->X where X is necessary for hid_hw_start() )
	status = hid_parse(dev);
	if (status) goto probe_fail;

	status = hid_hw_start(dev, HID_CONNECT_DEFAULT);
	if (status) goto probe_fail;

	// Log to kernel
	printk(KERN_INFO "HID Driver Bound:  Vendor ID: 0x%02x  Product ID: 0x%02x  Interface Num: 0x%02x\n", id->vendor, id->product, inum);
	printk(KERN_INFO "HID Device Info:  devnum: %d  devpath: %s\n", device->devnum, device->devpath);	// For debugging

	// Send a dummy report once
	if (inum == 0) {
		// cmd = generate_report(CMD_SET_LED);
		// cmd.data[1] = 0x0D;     // Green LED (profile indicicator)
		// cmd.data[2] = 0x01;     // ON

		cmd = generate_report(CMD_KBD_LAYOUT);
		out = send_command(dev, &cmd, &status);
		if (!status) log_report(&out);
	}

	return 0;

	probe_fail:
	if (device_data) kfree(device_data);
	printk(KERN_WARNING "Failed to start HID device: Razer Tartarus");
	return status;
}

// DEVICE DISCONNECT
// Clean up memory for private device data
static void device_disconnect (struct hid_device* dev) {
	// Get the device data
	struct keyboard_data* device_data = hid_get_drvdata(dev);

	// Stop the device 
	hid_hw_stop(dev);

	// Cleanup
	if (device_data) kfree(device_data);
	printk(KERN_INFO "HID Driver Unbound (Tartarus)\n");
}

// Some notes for developement:
//
//	The device seems to keep track of the order that the keys are
//	pressed which seems to be reflected in the event
//
//	Because of this, the trivial solution would be to simply
//	iterate the new report and check it against our existing
//	keylist (probably saved to the private sector)
//
//	If this is assumed and then the report is out of order,
//	all subsequent buttons would be registered as "unpressed"
// 	However, we should expect at most one key change per report,
//	so we could either detect this or make it our stopping condition
//
//	The current implementation will assume that this ordering is
//	guaranteed, as I can't reason why an HID event would be built
//	to provide reports in this way from the hardware side if not
//	for enabling optimal efficiency in polling inputs
//
//	If this proves problematic, I will seek to change this
//
//	TODO: Reference kernel implementation of event parsing
// ---

// Helper functions for event handling
void log_event (u8*, int);
int gen_keylist (u8*, int, u8*);
u8 key_action (struct keyboard_data*, int*, u8*, int);
u8 modkey_action (struct keyboard_data*, int*, u8);
// handle errors?
int key_index (u8);
void process_input (struct input_dev*, int, int, struct keymap*);


// DEVICE RAW EVENT
// Called upon any keypress/release
// (EV_KEY and available keys must be set in .input_configured)
// TODO DIFFERENT VERSIONS FOR KEYBOARD AND MOUSE VERSIONS
static int event_handler (struct hid_device* dev, struct hid_report* report, u8* data, int size) {
	u8 keylist[KEYLIST_LEN];
	int count;

	u8 key;
	int state;
	struct keyboard_data* device_data;

	int index;
	struct keymap* map;

	// todo something something switch for interface num

	device_data = hid_get_drvdata(dev);

	// log_event(data, size);	// DEBUG
	count = gen_keylist(data, size, keylist);
	if (count < 0) return -1;

	// Determine new button reported in the event (and its state)
	key = key_action(device_data, &state, keylist, count);
	if (!key) key = modkey_action(device_data, &state, *data);		// No key means modifier
	index = key_index(key);

	printk(KERN_INFO "Key Press:  key: 0x%02x (index: %d)  state: 0x%02x\n", key, index, state);

	// Send mapped input
	map = (device_data->hypershift)	? device_data->map_hs : device_data->map;
	// input = to_input_dev(dev->dev.parent);
	process_input(device_data->input, index, state, map);

	return 0;
}

// Log the output of a raw event for debugging
void log_event (u8* data, int size) {
	int i;
	int j;
	unsigned mask = 0x80;

	char* bits_str = (char*) kzalloc(9 * sizeof(char), GFP_KERNEL);
	char* data_str = (char*) kzalloc((10 * size + 1) * sizeof(char), GFP_KERNEL);

	if (!bits_str || !data_str) return;

	for (i = 0; i < size; i++) {
		for (j = 0; j < 8; j++)
			snprintf(bits_str + j, 2, "%u", !!(data[i] & (mask >> j)));
		snprintf(data_str + (i * 10), 11, "%s%s", bits_str, /*((i + 1) % 8) ? "  " : " \n"*/ "  ");
	}

	printk(KERN_INFO "RAW EVENT:  size: %d  data:\n\t%s\n", size, data_str);

	kfree(bits_str);
	kfree(data_str);
}

// Pull the raw keycodes from the event data
// 		data -> Raw event data
//		size -> Raw event size
//		keylist -> pointer to pre-allocated list
// Returns length of new list (will likely never exceed 6)
int gen_keylist (u8* data, int size, u8* keylist) {
	int i = 2;				// Iterator value (all standard keys reported from bytes 3 to 8)
	int count = 0;	 		// Number of keys reported in event
	u8 key;					// Current key at index

	if (!keylist) return -1;

	for ( ; i < size; i++) {
		if (!(key = data[i])) continue;
		keylist[count++] = key;
	}

	return count;
}

// Compare the new keylist with the old keylist (in the private sector)
// The private sector is then updated accordingly
// Returns the new keycode and sets the value of state to the value of the key
//	^^Preparation for registering actual device input to the system
//		dev -> HID device
//		state -> Pointer to int for secondary return
//		keylist -> New keylist parsed from the event
//		count -> Number of entries in the keylist
//
// TODO: If a discrepancy is found between the keylist lengths (more than one apart)
//		 Handle the situation by carefully looping through the data
u8 key_action (struct keyboard_data* data, int* state, u8* keylist, int count) {
	// struct keyboard_data* data = hid_get_drvdata(dev);

	int count_prev = data->keycount;
	u8* keylist_prev = data->keylist;

	int i = 0;			// Iterator val
	// int p = 0;		// Index placeholder for keylist_prev
	// int k = 0;		// Index placeholder for keylist (the one we are processing)
	u8 key = 0;

	// Ensure memory safety
	if (count > KEYLIST_LEN || count_prev > KEYLIST_LEN) return 0;

	switch (count - count_prev) {
	case 0:
		// No differences in pressed keys
		// (Likely more than 6 keys were pressed and our event does not support it)
		// 		^^potentially worth looking into in the future
		return 0;

	case -1:
		// A key was released
		*state = 0;

		// Iterate until the first unmatched key is found
		for ( ; i < count_prev; i++)
			if ((key = keylist_prev[i]) != keylist[i]) break;

		// If no key was found, take no action
		if (i == count_prev) return 0;

		// Remove found key from private data
		for ( ; i < count; i++)
			keylist_prev[i] = keylist_prev[i + 1];

		data->keycount = count;
		return key;

		// Return avoids an extra if-statement
		// return !!(i - (KEYLIST_LEN)) * key;
		
	case 1:
		// A key was pressed
		*state = 1;

		// Jump to new index
		key = keylist[count_prev];

		// Ensure key is not already in array
		// (Not the most efficient, but I am unsure of our ordering yet)
		// TODO: If our order is guaranteed, remove this section
		for ( ; i < count_prev; i++)
			if (keylist_prev[i] == key) return 0;

		keylist_prev[count_prev] = key;
		data->keycount = count;
		return key;
	
	default:
		// todo handle discrepancy
		// basically if weird shit happens and stuff is out of order...
		printk(KERN_WARNING "Razer Tartarus: Something wack-ass happened\n");
		return 0;
	}
}

u8 modkey_action (struct keyboard_data* data, int* state, u8 modkey) {
	u8 modkey_prev = data->modkey;
	u8 parsed = 0;		// Output key

	// Example:
	//      0000 0100	(alt held)
	// XOR  0000 0110	(shift pressed)
	//	   -----------
	//      0000 0010	(shift)

	switch (modkey_prev ^ modkey) {
	case MODKEY_SHIFT:
		parsed = MODKEY_MASK | MODKEY_SHIFT;
		*state = (modkey & MODKEY_SHIFT) ? 1 : 0;
		break;

	case MODKEY_ALT:
		parsed = MODKEY_MASK | MODKEY_ALT;
		*state = (modkey & MODKEY_ALT) ? 1 : 0;
		break;

	default:
		// Something weird happened (handle error by resetting modkeys)
		data->modkey = 0;
		return 0;
	}

	data->modkey = modkey;
	return parsed;
}

// Directly maps each device key to an array index (to save on memory)
// 2^8 indexes unmapped would require 256 entries of our keymap struct (*2 for hypershift)
// I'd swear there's a more practical way to write this, but I can't determine what that would be
int key_index (u8 key) {
	u8 ret = 0;

	switch (key) {
	// Keys 01 - 05
	case 0x1E:
		ret = 1;
		break;
	case 0x1F: 
		ret = 2;
		break;
	case 0x20:
		ret = 3;
		break;
	case 0x21:
		ret = 4;
		break;
	case 0x22:
		ret = 5;
		break;

	// Keys 06 - 10
	case 0x2B:
		ret = 6;
		break;
	case 0x14:
		ret = 7;
		break;
	case 0x1A:
		ret = 8;
		break;
	case 0x08:
		ret = 9;
		break;
	case 0x15:
		ret = 10;
		break;

	// Keys 11 - 15
	case 0x39:
		ret = 11;
		break;
	case 0x04:
		ret = 12;
		break;
	case 0x16:
		ret = 13;
		break;
	case 0x07:
		ret = 14;
		break;
	case 0x09:
		ret = 15;
		break;

	// Keys 16 - 20
	case 0x82:
		ret = 16;
		break;
	case 0x1D:
		ret = 17;
		break;
	case 0x1B:
		ret = 18;
		break;
	case 0x06:
		ret = 19;
		break;
	case 0x2C:
		ret = 20;
		break;

	// Unnamed circle, arrow key stick (l, u, r, d)
	case 0x84:
		ret = 21;
		break;
	case 0x50:
		ret = 22;
		break;
	case 0x52:
		ret = 23;
		break;
	case 0x4F:
		ret = 24;
		break;
	case 0x51:
		ret = 25;
		break;
	}

	return ret;
}

// Accesses the keymap and performs the specified input
// map should be the standard or hypershift keymap respectively
void process_input (struct input_dev* input, int index, int state, struct keymap* map) {
	struct keymap action;

	if (!map || index >= KEYMAP_LEN) return;

	action = map[index];
	switch (action.type) {
	case CTRL_KEYMAP:
		printk(KERN_INFO "process_input()  data: 0x%02x  state: 0x%02x\n", action.data, state);
		input_report_key(input, 0x1E, state);
		break;

	case CTRL_HYPERSHIFT:
		break;

	case CTRL_MACRO:
		break;

	case CTRL_PROFILE:
		break;

	default:
		// Take no action (CTRL_NOP jumps here)
		return;
	}
}