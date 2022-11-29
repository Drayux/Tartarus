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
// Struct for device private sector
//  ^^Specifically for keyboard device (inum 0x00)
// TODO DEVICE PROFILES (probably going to reset keymap/keymap_hypershift with a load function?)
// TODO private sector for mouse device
struct keyboard_data {
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


// -- DEVICE INTERFACING --
// Calculate report checksum
// (From OpenRazer)
unsigned char razer_checksum(struct razer_report* report) {
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
static struct razer_report send_command(struct hid_device* dev, struct razer_report* req, int* cmd_errno) {
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
	struct input_dev* input_dev = input->input;

	set_bit(EV_KEY, input_dev->evbit);
	for (int i = 0; i < 20; i++) set_bit(KEY_MACRO1 + i, input_dev->keybit);

	return 0;
}

// DEVICE PROBE
// Set up private device data
// Debug: Send sample URB for keyboard layout
static int device_probe(struct hid_device* dev, const struct hid_device_id* id) {
	int status;
	struct keyboard_data* device_data = NULL;

	struct razer_report cmd;
	struct razer_report out;

	struct usb_interface* interface = to_usb_interface(dev->dev.parent);
	struct usb_device* device = interface_to_usbdev(interface);

	u8 inum = interface->cur_altsetting->desc.bInterfaceNumber;

	// Prepare our data for the private sector
	// device_data = kzalloc(sizeof(struct keyboard_data), GFP_KERNEL);
	// hid_set_drvdata(dev, device_data);

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
		out = send_command(dev, &cmd, &status);
		if (!status) log_report(&out);
	}

	probe_fail:
	if (device_data) kfree(device_data);
	printk(KERN_WARNING "Failed to start HID device: Razer Tartarus");
		return status;

	return 0;
}

// DEVICE DISCONNECT
// Clean up memory for private device data
static void device_disconnect(struct hid_device* dev) {
	// Get the device data
	struct razer_data* device_data = hid_get_drvdata(dev);

	// Stop the device 
	hid_hw_stop(dev);

	// Cleanup
	if (device_data) kfree(device_data);
	printk(KERN_INFO "HID Driver Unbound (Tartarus)\n");
}

// DEVICE RAW EVENT
// Called upon any keypress/release
// (EV_KEY and available keys must be set in .input_configured)
static int event_handler (struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
	printk(KERN_INFO "Raw event handler called\n");
	return 0;
}