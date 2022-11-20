#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>      // This import might not be needed
#include <linux/hid.h>
#include <linux/string.h>   // For use with memset and memcpy

// -- DRIVER METADATA --
MODULE_AUTHOR("Drayux");
MODULE_DESCRIPTION("Tartarus V2 Driver");
MODULE_LICENSE("GPL");

// -- DEVICE SUPPORT --
#define VENDOR_ID	0x1532		// Razer USA, Ltd
#define PRODUCT_ID	0x022b		// Tartarus_V2

// -- DEVICE INTERFACING --
#define REPORT_LEN  0x5A        // Each USB report has 90 bytes
#define REPORT_IDX  0x01        // Report/Response (consistent across all devices so I don't think I need two defs)
#define WAIT_MIN    600         // Minmum response wait time is 600 microseconds (0.6 ms)
#define WAIT_MAX    800         // ^^Maximum is 800 us (0.8 ms)

// Commands
#define CMD_KBD_LAYOUT      0x00, 0x86, 0x02        // Query the device for its keyboard layout
#define CMD_SET_LED         0x03, 0x00, 0x03        // Set a specified LED with a given value

// Custom Keymapping
#define USAGE_IDX	90			// Highest index on the device
#define DEVICE_IDX	256			// Maximum usage index (aka how long our conversion map should be)

// Mapping events (u8 type)
#define CTRL_NOP		0x00	// No key action
#define CTRL_KEYMAP 	0x01	// Keyboard button action
#define CTRL_HYPERSHIFT 0x02	// Hypershift mode action	(TODO hold/toggle options)
#define CTRL_MACRO		0x03	// Play macro action 		(TODO play while held/toggle options)
#define CTRL_PROFILE	0x04	// Change profile action	(TODO any of this part lmaoo--gonna save it for last because I don't use it)

/*/ MISCELLANEOUS THINGS I MIGHT NEED

// HID Event types (https://www.kernel.org/doc/html/latest/input/event-codes.html)
EV_SYN: 		Used as markers to separate events. Events may be separated in time or in space, such as with the multitouch protocol.
EV_KEY: 		Used to describe state changes of keyboards, buttons, or other key-like devices.
EV_REL: 		Used to describe relative axis value changes, e.g. moving the mouse 5 units to the left.
EV_ABS: 		Used to describe absolute axis value changes, e.g. describing the coordinates of a touch on a touchscreen.
EV_MSC: 		Used to describe miscellaneous input data that do not fit into other types.
EV_SW: 			Used to describe binary state input switches.
EV_LED: 		Used to turn LEDs on devices on and off.
EV_SND: 		Used to output sound to devices.
EV_REP: 		Used for autorepeating devices.
EV_FF: 			Used to send force feedback commands to an input device.
EV_PWR: 		A special type for power button and switch input.
EV_FF_STATUS: 	Used to receive force feedback device status.


// LED definitions
#define ZERO_LED          0x00
// ...
#define RED_PROFILE_LED   0x0C
#define GREEN_PROFILE_LED 0x0D
#define BLUE_PROFILE_LED  0x0E
// ...


// This might just be used for FN-key handling (which I do not have)
struct razer_key_translation {
	u16 from;
	u16 to;
	u8 flags;
};


Mass event broadcasts:
Index	Key		Code	Lookup column
0x00 -> CTRL 	(0x1D)
0X01 -> LSHIFT 	(0x2A)
0X02 -> ALT		(0x38)
0X03 -> ]} ???	(0x7D) : shift / ctrl / shift caps / shift num
0X04 -> A  ???	(0x61) : ascii / num / shift caps
0X05 -> RSHIFT 	(0x36)
0X06 -> D  ???	(0x64) : ascii / num / shift caps
0X07 -> `~ ???	(0x7E) : shift / shift caps / shift num

Pretty sure the unknowns are: LWIN, RWIN, RCTRL, RALT

/*/

static int tartarus_probe(struct hid_device*, const struct hid_device_id*);
static void tartarus_disconnect(struct hid_device*);
static int event_handler(struct hid_device*, struct hid_field*, struct hid_usage*, int32_t);
// static int tartarus_input_mapping(struct hid_device* dev, struct hid_input* input, struct hid_field* field,
// 							 struct hid_usage* usage, unsigned long** bit, int* max) { return 0; }

// Array of USB device ID structs
static struct hid_device_id id_table [] = {
	{ HID_USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ 0 }
}; MODULE_DEVICE_TABLE(hid, id_table);

// Driver metadata struct
static struct hid_driver tartarus_driver = {
	.name = "razer_tartarus_usb",
	.id_table = id_table,
	.probe = tartarus_probe,
	.remove = tartarus_disconnect,
	.event = event_handler
	// .input_mapping = tartarus_input_mapping
};

module_hid_driver(tartarus_driver);


// -- DATATYPES --
// Struct for device private sector
// TODO DEVICE PROFILES (probably going to reset keymap/keymap_hypershift with a load function?)
struct razer_data {
	int32_t hypershift;
	unsigned keymap[USAGE_IDX];
	unsigned keymap_hypershift[USAGE_IDX];
};

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

// Create a razer report data type
// Specifically for use in requesting data from the device
// (Core operation taken from OpenRazer)
struct razer_report generate_report(unsigned char class, unsigned char id, unsigned char size) {
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

// Log the a razer report struct to the kernel (currently used for debugging)
void log_report(struct razer_report* report) {
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
		// snprintf(params_str + (3 * i), 7, "%02x%02x%s", (char) i, (char) (i + 1), ((i + 2) % 8) ? "  " : "\n\t");

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


// -- DEVICE INTERFACING (main driver goodness) --
// Calculate report checksum
// (Taken from OpenRazer)
unsigned char razer_checksum(struct razer_report* report) {
	unsigned char ck = 0;
	unsigned char* bytes = (unsigned char*) report;

	for (int i = 2; i < 88; i++) ck ^= bytes[i];
	return ck;
}

// Monolithic function to communcate with device
// Returns the device response
// Use req (aka the request report) to specify the command to send
// cmd_errno can be NULL (but shouldn't be)
// Core functionality taken from OpenRazer
static struct razer_report send_command(struct hid_device* dev, struct razer_report* req, int* cmd_errno) {
	char* request;      // razer_report containing the command parameters (i.e. get layout/set lighting pattern/etc)
	int received = -1;  // Amount of data transferred (result of usb_control_msg)

	struct usb_interface* parent = to_usb_interface(dev->dev.parent);
	struct usb_device* tartarus = interface_to_usbdev(parent);

	// Function output
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

// Called when the device is bound
// TODO: Hypershift mode needs a device file probably?
static int tartarus_probe(struct hid_device* dev, const struct hid_device_id* id) {
	int status;
	struct razer_data* device_data = NULL;
	unsigned* keymap;
	unsigned* keymap_hs;

	struct razer_report cmd;
	struct razer_report out;

	// Cast our HID device to a USB device
	// My current understanding of the hierarchy:
	//  usb_device is the full representation of the device to the kernel
	//  usb_interface is a "sub-device" of a usb_device
	//    The parent does not have a list of its interfaces, but instead
	//      its children are given pointers to their parent when detected by the kernel
	//    These represent the (possibly) multiple interfaces within the usb_device
	//  hid_device is a child of a usb_interface
	struct usb_interface* interface = to_usb_interface(dev->dev.parent);
	struct usb_device* device = interface_to_usbdev(interface);

	u8 inum = interface->cur_altsetting->desc.bInterfaceNumber;

	// Prepare our data for the private sector
	device_data = kzalloc(sizeof(struct razer_data), GFP_KERNEL);
	hid_set_drvdata(dev, device_data);

	// Prepare the device keymap
	device_data->hypershift = 0;

	// TODO: parse_keymap(<device_data>, <file>, ...)
	keymap = device_data->keymap;
	memset(keymap, 0, sizeof(unsigned) * USAGE_IDX);

	// Binds (index is 'usage_index', value is desired key scancode)
	// TODO STRUCT REFACTOR:
	//	Instead of mapping directly to scan codes, map to a struct to specify direct codes, hypershift codes, or macros (would also solve the hypershift/hypershift button issue)
	// Decent list of scan codes: https://flint.cs.yale.edu/cs422/doc/art-of-asm/pdf/APNDXC.PDF
	// Hard-coded is my preferred config
	keymap[0x1e] = 0x02;		// Key 01 -> 1
	keymap[0x1f] = 0x03;		// Key 02 -> 2
	keymap[0x20] = 0x04;		// Key 03 -> 3
	keymap[0x21] = 0x05;		// Key 04 -> 4
	keymap[0x22] = 0x06;		// Key 05 -> 5

	keymap[0x2b] = 0x01;		// Key 06 -> ESC
	keymap[0x14] = 0x10;		// Key 07 -> Q
	keymap[0x1a] = 0x11;		// Key 08 -> W
	keymap[0x08] = 0x12;		// Key 09 -> E
	keymap[0x15] = 0x13;		// Key 10 -> R

	keymap[0x39] = 0x2b;		// Key 11 -> BACKSLASH
	keymap[0x04] = 0x1e;		// Key 12 -> A
	keymap[0x16] = 0x1f;		// Key 13 -> S
	keymap[0x07] = 0x20;		// Key 14 -> D
	keymap[0x09] = 0x21;		// Key 15 -> F

	keymap[0x01] = 0x2a;		// Key 16 -> SHIFT
	keymap[0x1d] = 0x2c;		// Key 17 -> Z
	keymap[0x1b] = 0x2d;		// Key 18 -> X
	keymap[0x06] = 0x2e;		// Key 19 -> C

	// keymap[0x02] = 0xfe;		// Thumb Button -> Hypershift (0xfe is not a scan code but an override for us)
								//	I could consider moving this to it's own field and checking accordingly
	keymap[0x52] = 0x48;		// Thumb Hat (U) -> Arrow UP
	keymap[0x4f] = 0x4d;		// Thumb Hat (R) -> Arrow RIGHT
	keymap[0x51] = 0x50;		// Thumb Hat (D) -> Arrow DOWN
	keymap[0x50] = 0x4b;		// Thumb Hat (L) -> Arrow LEFT

	keymap[0x2c] = 0x39;		// Key 20 -> SPACE

	// Copy the default profile so the hypershift is just a modification
	// keymap_hs = device_data->keymap_hypershift;
	// memcpy(keymap_hs, keymap, sizeof(unsigned) * USAGE_IDX);

	// Hypershift binds
	// keymap_hs[0x1e] = 0x07;		// Key 01 -> 6
	// keymap_hs[0x1f] = 0x08;		// Key 02 -> 7
	// keymap_hs[0x20] = 0x09;		// Key 03 -> 8
	// keymap_hs[0x21] = 0x0a;		// Key 04 -> 9
	// keymap_hs[0x22] = 0x0c;		// Key 05 -> ~

	// Attempt to start communication with the device
	status = hid_parse(dev);		// I think this populates dev->X where X is necessary for hid_hw_start()
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
		out = send_command(dev, &cmd, NULL);
		log_report(&out);
	}

	return 0;

	probe_fail:
	if (device_data) kfree(device_data);
	printk(KERN_WARNING "Failed to start HID device: Razer Tartarus");
		return status;
}

// Called when the device is unbound
static void tartarus_disconnect(struct hid_device* dev) {
	// Get the device data
	struct razer_data* device_data = hid_get_drvdata(dev);

	// Stop the device 
	// if (!(*device_data)) hid_hw_stop(dev);
	hid_hw_stop(dev);

	// Memory cleanup
	if (device_data) kfree(device_data);

	printk(KERN_INFO "HID Driver Unbound (Tartarus)\n");
}

// Called in response to every HID event (beginning after hid_hw_start() in probe())
// Useful links (for my personal reference lmaooo)
// [EVENT HANDLER]  https://elixir.bootlin.com/linux/latest/source/drivers/hid/hid-core.c#L1507
// [INPUT PARSER]   https://elixir.bootlin.com/linux/latest/source/drivers/hid/hid-input.c#L1443
// [INPUT EVENT]  	https://elixir.bootlin.com/linux/latest/source/drivers/input/input.c#L423
// [HID DRIVER]		https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h#L776
// It could be worth attempting to use the input mapping feature, however this might make macros and hypershift less practical?
static int event_handler(struct hid_device* dev, struct hid_field* field, struct hid_usage* usage, int32_t value) {
	struct input_dev *input;
	struct razer_data* device_data = hid_get_drvdata(dev);
	unsigned idx;
	unsigned code;
	unsigned* keymap;

	// If we have no device data yet, just perform the default function
	if (!device_data) return 0;

	// If the request is missing data, raise an error (not sure what to do here--just want memory safety)
	if (!field || !field->hidinput || !usage->type) return -1;
	input = field->hidinput->input;

	// TODO: There are some "extra" events for every main event, usage_index 0 -> 7 are sent with value 0 every event
	// Prune non keyboard types (right now mouse functions pass through)
	//	Keyboard is type 0x01
	// 	Mouse is type 0x02
	// if (usage->type != 0x01) return 0;	// Disabled for debugging

	// Some testing to figure out codes
	/*if (value)*/
	printk(KERN_INFO "Event Info:  type: 0x%02x  code: 0x%02x  value: 0x%02x  index: 0x%02x  hid: 0x%08x\n", usage->type, usage->code, value, usage->usage_index, usage->hid);
	if (usage->usage_index > 0x07) printk("\n");

	// Lookup our keycode and send the data
	idx = usage->usage_index;
	if (idx >= DEVICE_IDX) return -1;	// This should never be called but for the sake of due diligence

	// TODO Hypershift could drop input releases (press button, press HS, release button, release HS)

	// Grab the pointer we want depending if we have hypershift on or not
	// keymap = (device_data->hypershift) ? device_data->keymap_hypershift : device_data->keymap;
	// keymap = device_data->keymap;
	// code = keymap[idx];
	// if (code == 0x00) return 1;				// 0x00 means no mapping
	// if (code == 0xff) code = usage->code;	// 0xff means default mapping
	// else if (code == 0xfe) {					// 0xfe is hypershift
	// 	device_data->hypershift = value;
	// 	return 1;
	// }
	// if (code == 0x00) code = usage->code;

	// input_event(input, 0x01, code, value);
	input_event(input, usage->type, usage->code, value);
    return 1;
}
