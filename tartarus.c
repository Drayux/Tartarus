#include <linux/string.h>   // memset() and memcpy()
#include <linux/usb.h>      // URB types

#include "module.h"			// Module and device defines
#include "keymap.h"			// TEMPORARY HARD-CODED PROFILE

// -- DEVICE EVENTS --
// Probe called upon device detection (initalization step)
static int device_probe(struct hid_device* dev, const struct hid_device_id* id) {
	int status;

	struct usb_interface* intf = to_usb_interface(dev->dev.parent);
	// struct usb_device* usb = interface_to_usbdev(intf);		// Used only for debug output
	u8 inum = intf->cur_altsetting->desc.bInterfaceNumber;

	struct drvdata* data = NULL;
	struct kbddata* kdata = NULL;
	// struct mousedata* mdata = NULL;
	void* idata = NULL;

	// struct razer_report cmd;		// (debugging)
	// struct razer_report out;		// (debugging)
	// printk(KERN_INFO "Attempting to initalize Tartarus HID driver (0x%02x)\n", inum);	// (debugging)

	// Initalize driver data
	switch (inum) {
	case 1:
		// TODO: Not sure what this interface is used for
		//     ^^Just save the inum and exit
		data = kzalloc(sizeof(struct drvdata), GFP_KERNEL);
		if ((status = data ? 0 : -ENOMEM)) goto probe_fail;
		
		data->inum = inum;
		hid_set_drvdata(dev, data);
		
		return 0;
		
	case 0:
		// Keyboard
		idata = kzalloc(sizeof(struct kbddata), GFP_KERNEL);
		if ((status = idata ? 0 : -ENOMEM)) goto probe_fail;

		/*/ Send a dummy device command (debugging) 
		cmd = init_report(CMD_KBD_LAYOUT);
		out = send_command(dev->dev.parent, &cmd, &status);
		if (!status) log_report(&out);
		//*/

		// Manually set keymap (debugging)
		kdata = idata;		// TODO: Remove this if we do not need to set kb-specific fields
		memcpy(kdata->maps[0].keymap, default_keymap, sizeof(struct profile));
		//*/
		
		// Create device files
		// TODO: Needs proper error handling (currently will leak idata)
		if(device_create_file(&dev->dev, &dev_attr_profile_num)) return -1;
		if(device_create_file(&dev->dev, &dev_attr_profile)) return -1;

		break;
		
	case 2:
		// "Mouse"
		idata = kzalloc(sizeof(struct mousedata), GFP_KERNEL);
		if ((status = idata ? 0 : -ENOMEM)) goto probe_fail;

		// mdata = idata;
		
		// if(device_create_file(&dev->dev, &dev_attr_profile_num)) return -1;
		// if(device_create_file(&dev->dev, &dev_attr_profile)) return -1;
		break;
	}

	data = kzalloc(sizeof(struct drvdata), GFP_KERNEL);
	if ((status = data ? 0 : -ENOMEM)) goto probe_fail;

	mutex_init(&data->lock);
	data->inum = inum;
	data->idata = idata;
	data->profile = 1;		// (DEBUGGING)
	hid_set_drvdata(dev, data);

	// Begin device communication
	// NOTE: Device must be fully prepared before hid_hw_start() (including dev_drvdata)
	if ((status = hid_parse(dev))) goto probe_fail;
	if ((status = hid_hw_start(dev, HID_CONNECT_DEFAULT))) goto probe_fail;

	// Log success to kernel
	printk(KERN_INFO "HID Driver Bound:  Vendor ID: 0x%02x  Product ID: 0x%02x  Interface Num: 0x%02x\n", id->vendor, id->product, inum);
	// printk(KERN_INFO "HID Device Info:  devnum: %d  devpath: %s\n", usb->devnum, usb->devpath);	// (debugging)

	return 0;

probe_fail:
	if (idata) kfree(idata);
	if (data) kfree(data);
	printk(KERN_WARNING "Failed to start HID driver: Razer Tartarus v2 (0x%02x)\n", inum);
	return status;
}

// Define device input parameters (input_dev event types, available keys, etc.)
// NOTE: Called by kernel during hid_hw_start() (insde device probe)
static int input_config(struct hid_device* dev, struct hid_input* input) {
	struct input_dev* input_dev = input->input;
	struct drvdata* dev_data = hid_get_drvdata(dev);
	if (!dev_data) return -1;	// TODO: We might want to use a different errno

	// Save our input device for later use (can be cast but this is easier)
	// Thanks: https://github.com/nirenjan/libx52/blob/482c5980abd865106a418853783692346e19ebb6/kernel_module/hid-saitek-x52.c#L124
	dev_data->input = input_dev;

	// Specify our input conditions
	switch (dev_data->inum) {
	case 0:
		// Keyboard
		set_bit(EV_KEY, input_dev->evbit);
		set_bit(1, input_dev->keybit);		// I still don't understand this

		// https://elixir.bootlin.com/linux/v6.7/source/include/uapi/linux/input-event-codes.h#L65
		// TODO: Are these keys that trigger events or possible outputs?
		// for (int i = 1; i <= 127; i++) set_bit(i, input_dev->keybit);
		// for (int i = 183; i <= 194; i++) set_bit(i, input_dev->keybit);
		
		break;
	
	case 2:
		// "Mouse"
		set_bit(EV_REL, input_dev->evbit);
		set_bit(BTN_MIDDLE, input_dev->keybit);
		set_bit(BTN_WHEEL, input_dev->keybit);

		// Might need this one?
		// set_bit(REL_WHEEL, input_dev->relbit);

		break;
	}
	
	return 0;
}

// Cleanup resources when device is disconnected
static void device_disconnect(struct hid_device* dev) {
	// Get the device data
	struct drvdata* data = hid_get_drvdata(dev);
	void* idata;

	// No device data, something probably went wrong
	if (!data) return;

	switch (data->inum) {
	case 1:
		// Mysterious interface
		kfree(data);
		return;
	case 0:
		// Keyboard
		device_remove_file(&dev->dev, &dev_attr_profile_num);
		device_remove_file(&dev->dev, &dev_attr_profile);
		break;
	case 2:
		// Mouse
		// device_remove_file(&dev->dev, &dev_attr_profile);
		break;
	}

	// Stop the device 
	hid_hw_stop(dev);

	// Cleanup
	if ((idata = data->idata)) kfree(idata);
	kfree(data);

	printk(KERN_INFO "HID Driver Unbound (Razer Tartarus v2)\n");
}

// Called upon any keypress/release
// NOTE: EV_KEY and available keys must be set in .input_configured
static int handle_event(struct hid_device* dev, struct hid_report* report, u8* event, int len) {
	/*/ Some notes for developement:

		The device seems to keep track of the order that the keys are
		pressed which seems to be reflected in the event

		Because of this, the trivial solution would be to simply
		iterate the new report and check it against our existing
		keylist

		If this is assumed and then the report is out of order,
		all subsequent buttons would be registered as "unpressed"
		However, we should expect at most one key change per report,
		so we could either detect this or make it our stopping condition

		The current implementation will assume that this ordering is
		guaranteed, as I can't reason why an HID event would be built
		to provide reports in this way from the hardware side if not
		for enabling optimal efficiency in polling inputs

		If this proves problematic, I will seek to change this

		TODO: Reference kernel implementation of event parsing
		https://elixir.bootlin.com/linux/v6.0.11/source/drivers/hid/usbhid/usbkbd.c#L117
	/*/

	struct drvdata* data = hid_get_drvdata(dev);
	struct bind action = { 0 };
	int state = -1;

	// log_event(event, len, (data) ? data->inum : 0xFF); 	// (DEBUGGING)

	if (!data) return -1;				// TODO: Different errno may be desirable
	if (!data->profile) return 0;		// Device "disabled" take no action

	// TODO: Lock the mutex here!
	switch (data->inum) {
	case 0:
		action = key_event(data->idata, data->profile - 1, &state, event, len);
		printk(KERN_INFO "Action type: 0x%02x ; Action data: 0x%02x ; State: %d\n", action.type, action.data, state);
		break;

	case 2:
		printk(KERN_INFO "Mouse event detected!\n");
		break;
	}

	if (state < 0) action.type = CTRL_NOP;

	switch (action.type) {
	case CTRL_KEY:
		input_report_key(data->input, action.data, state);
		break;

	case CTRL_SHIFT:
		break;

	case CTRL_PROFILE:
		break;

	case CTRL_MACRO: break;
	}
	// TODO: Unlock mutex here!

	return 0;
}

// Currently selected device profile (decimal string)
// Change keyboard profile (updates lights and "releases" keys)
void set_profile_num(struct device* idev, u8 profile) {
	// struct razer_report cmd;
	// struct razer_report out;
	// int status = 0;

	struct drvdata* data = dev_get_drvdata(idev);

	// Multiple threads could attempt this routine simultaneously
	mutex_lock(&data->lock);

	// Regular profiles should be 1 -> 8, or 0
	// Values such as say 11 or 12 will map to 3 or 4 respectively
	data->profile = ((profile - 1) % PROFILE_COUNT + 1) * !!profile;

	/*/ Set profile indicator lights
	//	TODO: This seems buggy to send 3 URBs in succession....perhaps even breaking my USB port?
	cmd = init_report(CMD_SET_LED);
	cmd.data[0] = 0x01;		// Variable store? (we may want this to be 0 instead)

	// Blue profile led (1)
	cmd.data[1] = 0x0E; 	// https://github.com/openrazer/openrazer/blob/master/driver/razercommon.h#L57
	cmd.data[2] = profile & 0x01;
	out = send_command(idev->parent, &cmd, &status);

	// Green profile led (2)
	cmd.data[1] = 0x0D; 	// https://github.com/openrazer/openrazer/blob/master/driver/razercommon.h#L56
	cmd.data[2] = !!(profile & 0x02);
	out = send_command(idev->parent, &cmd, &status);

	// Red profile led (4)
	cmd.data[1] = 0x0C; 	// https://github.com/openrazer/openrazer/blob/master/driver/razercommon.h#L55
	cmd.data[2] = !!(profile & 0x04);
	out = send_command(idev->parent, &cmd, &status);

	// if (!status) log_report(&out);
	//*/

	// TODO: Update keypresses (release all keys with current profile and press again with new)

	printk(KERN_INFO "Set tartarus profile to %d\n", data->profile);
	mutex_unlock(&data->lock);
}

// NOTE: buf points to an array of PAGE_SIZE (or 4096 bytes on x86)
static ssize_t profile_num_show(struct device* dev, struct device_attribute* attr, char* buf) {
	struct drvdata* data = dev_get_drvdata(dev);
	return snprintf(buf, 4, "%d", data->profile); 
}

static ssize_t profile_num_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len) {
	unsigned long profile;
	int status;

	// Add a null terminator to the input string
	char nbuf[4] = { 0 };
	memcpy(nbuf, buf, (len > 3) ? 3 : len);		// nbuf[3] will always remain 0
	
	// Convert the provided (base 10) string into a number
	status = kstrtoul(nbuf, 10, &profile);
	if (status) { 
		printk(KERN_WARNING "Tartarus: Unable to convert sysfs profile value '%s'\n", nbuf);
		return len;
	}
	
	set_profile_num(dev, profile % 0xFF);
	return len;
}

// Manage the device profile (keymap) itself
// Outputs the map of the currently selected profile
static ssize_t profile_show(struct device* dev, struct device_attribute* attr, char* buf) {
	size_t len = 0;
	struct drvdata* data = dev_get_drvdata(dev);
	struct kbddata* kdata;
	u8 profile;
	// struct mousedata* mdata;

	mutex_lock(&data->lock);
	switch (data->inum) {
	case 1: break;
	case 0:
		// Keyboard
		profile = data->profile;
		if (!profile) break;		// Profile 0 reserved for "no profile"
		
		len = sizeof(struct profile);
		kdata = data->idata;
		memcpy(buf, kdata->maps + profile - 1, len);
		
		break;

	case 2:
		// Mouse
		// TODO
		// mdata = data->idata;
		// profile = data->profile;		
		// len = sizeof(struct mprofile);
		break;
	}

	mutex_unlock(&data->lock);
	return len;
}

static ssize_t profile_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len) {
	return len;
}


// -- INPUT PROCESSING --
// Log the output of a raw event for debugging
void log_event(u8* data, int len_data, u8 inum) {
	int i;
	int j;
	unsigned mask = 0x80;

	char* bits_str = kzalloc(9 * sizeof(char), GFP_KERNEL);
	char* data_str = kzalloc((10 * len_data + 1) * sizeof(char), GFP_KERNEL);

	// I'm kinda cheating but this won't go into the "production build" of the module
	if (!bits_str || !data_str) {
		printk(KERN_WARNING "Ran out of memory while logging device event (inum: %d)\n", inum);
		return;
	}

	for (i = 0; i < len_data; i++) {
		for (j = 0; j < 8; j++)
			snprintf(bits_str + j, 2, "%u", !!(data[i] & (mask >> j)));
		snprintf(data_str + (i * 10), 11, "%s%s", bits_str, /*((i + 1) % 8) ? "  " : " \n"*/ "  ");
	}

	printk(KERN_INFO "RAW EVENT:  inum: %d  size: %d  data:\n\t%s\n", inum, len_data, data_str);

	kfree(bits_str);
	kfree(data_str);
}

// Core handling function for keyboard event
// NOTE: If implementing double-binds, that would be added here
struct bind key_event(struct kbddata* data, u8 pnum, int* pstate, u8* event, int len) {
	union keystate state_new = { 0 };
	struct bind action = { 0 };

	int i;
	u8 key = 0;		// Optional intermediate value for my sanity
	u32 state = 0;

	struct profile* map;

	// Determine state change (if any)
	// TODO: Assert that len is 8
	for (i = 2; i < len; ++i) {
		if (!(key = event[i])) break;			// Key-presses always start at 2 and are listed sequentially
		state_new.b[key / 8] |= 1 << (key % 8);
	}

	// NOTE: We assume that there will be at most one different key
	for (i = 0; i < 8; ++i) {
		state = state_new.comp[i] ^ data->state.comp[i];

		// Kinda scuffed log_2 but probably more efficient than a while loop
		// TODO: I think this ONLY works on little-endian architectures
		switch (state) {
			case 0x00000001: key =  0; break;
			case 0x00000002: key =  1; break;
			case 0x00000004: key =  2; break;
			case 0x00000008: key =  3; break;

			case 0x00000010: key =  4; break;
			case 0x00000020: key =  5; break;
			case 0x00000040: key =  6; break;
			case 0x00000080: key =  7; break;

			case 0x00000100: key =  8; break;
			case 0x00000200: key =  9; break;
			case 0x00000400: key = 10; break;
			case 0x00000800: key = 11; break;

			case 0x00001000: key = 12; break;
			case 0x00002000: key = 13; break;
			case 0x00004000: key = 14; break;
			case 0x00008000: key = 15; break;

			case 0x00010000: key = 16; break;
			case 0x00020000: key = 17; break;
			case 0x00040000: key = 18; break;
			case 0x00080000: key = 19; break;

			case 0x00100000: key = 20; break;
			case 0x00200000: key = 21; break;
			case 0x00400000: key = 22; break;
			case 0x00800000: key = 23; break;

			case 0x01000000: key = 24; break;
			case 0x02000000: key = 25; break;
			case 0x04000000: key = 26; break;
			case 0x08000000: key = 27; break;

			case 0x10000000: key = 28; break;
			case 0x20000000: key = 29; break;
			case 0x40000000: key = 30; break;
			case 0x80000000: key = 31; break;

			// No differences (or the firmware broke lmaooo)
			default: continue;
		}

		key += i * 32;
		break;
	}

	if (key) {
		data->state.b[key / 8] ^= 1 << (key % 8);
		*pstate = !!(state_new.b[key / 8] & 1 << (key % 8));
		
	// Event was not a normal key, check modifier keys
	} else {
		//      0000 0100	(alt held)
		// XOR  0000 0110	(shift pressed)
		//	   -----------
		//      0000 0010	(shift)

		key = event[0] ^ data->modkey;
		if (!key) return action;

		data->modkey ^= key;
		*pstate = !!(event[0] & key);

		// Convert the modifier key bit into a unique key value
		// Said key values have been verified not to conflict with existing values
		// ^^Alt = 0x44 ; Shift = 0x42
		key |= MODKEY_MASK;
	}

	// Look up device profile to determine keybind
	map = data->maps + pnum;
	action = map->keymap[key];

	// printk(KERN_INFO "Key index: 0x%02x (0x%02x) ; state: %d\n", key, action.data, *pstate);

	return action;
}

/*/ Directly maps each device key to an array index
// ^^(saves on memory and makes profiles more intuitive)
int key_index_old(u8 key) {
	switch (key) {

		// Keys 01 - 05
		case 0x1E: return 1;
		case 0x1F: return 2;
		case 0x20: return 3;
		case 0x21: return 4;
		case 0x22: return 5;

		// Keys 06 - 10
		case 0x2B: return 6;
		case 0x14: return 7;
		case 0x1A: return 8;
		case 0x08: return 9;
		case 0x15: return 10;

		// Keys 11 - 15
		case 0x39: return 11;
		case 0x04: return 12;
		case 0x16: return 13;
		case 0x07: return 14;
		case 0x09: return 15;

		// Keys 16 - 20
		case 0x82: return 16;
		case 0x1D: return 17;
		case 0x1B: return 18;
		case 0x06: return 19;
		case 0x2C: return 20;

		// Unnamed circle, arrow key stick (l, u, r, d)
		case 0x84: return 21;
		case 0x50: return 22;
		case 0x52: return 23;
		case 0x4F: return 24;
		case 0x51: return 25;

	} return 0;
} //*/


// -- DEVICE COMMANDS --
// Log the a razer report struct to the kernel (for debugging)
void log_report(struct razer_report* report) {
	char* status_str;
	char* params_str;
	int i;

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
		status_str = "Unexpected status (0x??)";
	};
	
	// Command params (data)
	// Length of param string: 80 * 2 hex chars, + (80 / 2) * 2 spaces + 1 null = 241
	params_str = kzalloc(241 * sizeof(char), GFP_KERNEL);
	if (!params_str) return;
	for (i = 0; i < 80; i += 2)
		snprintf(params_str + (3 * i), 7, "%02x%02x%s", report->data[i], report->data[i + 1], ((i + 2) % 16) ? "  " : "\n\t");

	printk(KERN_INFO "TARTARUS DEBUG INFORMATION:\n\n				\
		\tTransaction ID: 0x%02x									\
		\tStatus: %s\n												\
		\tCommand ID: 0x%02x										\
		\tClass: 0x%02x\n											\
		\tSize: 0x%02x (%d)\t\tRemaining Packets: 0x%02x (%d)\n\n	\
		\tData: 0x\n\t%s\n",
		   report->tr_id.id,
		   status_str,
		   report->cmd_id.id,
		   report->class,
		   report->size, report->size,
		   report->remaining, report->remaining,
		   params_str);

	kfree(params_str);
}

// Calculate report checksum (razer_checksum)
// (Taken from OpenRazer driver)
unsigned char report_checksum(struct razer_report* report) {
	unsigned char ck = 0;
	unsigned char* bytes = (unsigned char*) report;

	for (int i = 2; i < 88; i++) ck ^= bytes[i];
	return ck;
}

// Create a razer report data type
// Specifically for use in requesting data from the device
// (Modified from OpenRazer)
struct razer_report init_report(unsigned char class, unsigned char id, unsigned char size) {
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

// Send prepared device URB
// Returns the device response
// dev needs to be the tartarus itself (parent of the interfaces to which the driver is bound)
// Use req (aka the request report) to specify the command to send
// cmd_errno can be NULL (but shouldn't be)
// (Modified from OpenRazer driver)
struct razer_report send_command(struct device* dev, struct razer_report* req, int* cmd_errno) {
	int received = -1;		// Amount of data transferred (result of usb_control_msg)
	char* request = NULL;	// razer_report containing the command parameters (i.e. get layout/set lighting pattern/etc)

	struct razer_report response = { 0 };

	// struct usb_interface* intf = to_usb_interface(dev->dev.parent);
	struct usb_interface* intf = to_usb_interface(dev);
	struct usb_device* tartarus = interface_to_usbdev(intf);

	// Allocate necessary memory and check for errors
	request = (char*) kzalloc(sizeof(struct razer_report), GFP_KERNEL);
	if (!request) {
		printk(KERN_WARNING "Failed to communcate with device: Out of memory.\n");
		if (cmd_errno) *cmd_errno = -ENOMEM;
		return response;
	}

	// Copy data from our request struct to the fresh pointer
	req->cksum = report_checksum(req);
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
	// TODO: Do I need to use usleep_range again?
	if (received != REPORT_LEN) {
		// We've already made the attempt so return what we've got
		printk(KERN_WARNING "Invalid device data transfer. (%d bytes != %d bytes)\n", received, REPORT_LEN);
		if (cmd_errno) *cmd_errno = received;
	}

	// We aren't referencing the buffer so we copy it to the stack
	memcpy(&response, request, sizeof(struct razer_report));

send_command_exit:
	kfree(request);
	return response;
}
