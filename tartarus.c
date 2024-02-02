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
	case EXT_INUM:
		// TODO: Not sure what this interface is used for
		//     ^^Just save the inum and exit
		data = kzalloc(sizeof(struct drvdata), GFP_KERNEL);
		if ((status = data ? 0 : -ENOMEM)) goto probe_fail;
		
		data->inum = inum;
		hid_set_drvdata(dev, data);
		
		return 0;
		/*/
		break;
		//*/
		
	case KBD_INUM:
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
		memcpy(kdata->maps[1].keymap, default_keymap_shift, sizeof(struct profile));
		//*/
		
		// Create device files
		// TODO: Needs proper error handling (currently will leak idata)
		if(device_create_file(&dev->dev, &dev_attr_profile_num)) return -1;
		if(device_create_file(&dev->dev, &dev_attr_profile)) return -1;

		break;
		
	case MOUSE_INUM:
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
	printk(KERN_INFO "HID Tartarus: Successfully bound device driver\n\tVendor ID: 0x%02x  Product ID: 0x%02x  Interface Num: 0x%02x\n", id->vendor, id->product, inum);
	// printk(KERN_INFO "HID Device Info:  devnum: %d  devpath: %s\n", usb->devnum, usb->devpath);	// (debugging)

	return 0;

probe_fail:
	if (idata) kfree(idata);
	if (data) kfree(data);
	printk(KERN_WARNING "HID Tartarus: Failed to initalize driver (status: 0x%02x)\n", inum);
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
	case EXT_INUM:		// Should never run but this is nondestructive if so
	case KBD_INUM:
		// Keyboard (https://elixir.bootlin.com/linux/v6.7.2/source/drivers/input/input.c#L432)
		// KEYCODES: https://elixir.bootlin.com/linux/v6.7/source/include/uapi/linux/input-event-codes.h#L65
		set_bit(EV_KEY, input_dev->evbit);
		for (int i = 1; i <= 248; ++i) set_bit(i, input_dev->keybit);
		break;
	
	case MOUSE_INUM:
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
	case EXT_INUM:
		// Mysterious interface
		kfree(data);
		return;
	case KBD_INUM:
		// Keyboard
		device_remove_file(&dev->dev, &dev_attr_profile_num);
		device_remove_file(&dev->dev, &dev_attr_profile);
		break;
	case MOUSE_INUM:
		// Mouse
		// device_remove_file(&dev->dev, &dev_attr_profile);
		break;
	}

	// Stop the device 
	hid_hw_stop(dev);

	// Cleanup
	if ((idata = data->idata)) kfree(idata);
	kfree(data);

	printk(KERN_INFO "HID Tartarus: Driver unbound\n");
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
	// struct kbddata* kdata;
	struct bind action = { 0 };
	int state = -1;

	// log_event(event, len, (data) ? data->inum : 0xFF); 	// (DEBUGGING)

	if (!data) return -1;				// TODO: Different errno may be desirable (we might also just assume that we have data at this point)

	// We use a mutex here because some keys change the device profile
	// As a result, it would be possible to press a key and release a different key	
	mutex_lock(&data->lock);
	
	switch (data->inum) {
	case KBD_INUM:
		action = key_event(data->idata, data->profile, &state, event, len);
		break;

	case MOUSE_INUM:
		action.type = CTRL_MWHEEL;
		break;
	}

	// TODO: Error handling (this was the old solution)
	// if (state < 0) action.type = CTRL_NOP;

	// Debug output
	// printk(KERN_INFO "Key index: 0x%02x (0x%02x) ; state: %d\n", key, action.data, state);
	// printk(KERN_INFO "Action type: 0x%02x ; Action data: 0x%02x ; State: %d\n", action.type, action.data, state);

	switch (action.type) {
	case CTRL_KEY:
		input_report_key(data->input, action.data, state);
		break;

	case CTRL_SHIFT:
		// Shift mode only supported by keyboard buttons (mwheel could technically work but holy scuffed)
		if (data->inum != KBD_INUM) break;
		if (action.data) swap_profile_kbd(dev->dev.parent, data, action.data);
		break;

	case CTRL_PROFILE:
		// Profile swaps from within hypershift mode will reset hypershift mode
		// NOTE: Only key presses will make it here (else we'd also check for state in our swap_profile_kbd condition)

		// TODO: Figure out mouse support for this (probably involves linking to the other device data?)
		if (data->inum != KBD_INUM) break;
		if (action.data) swap_profile_kbd(dev->dev.parent, data, action.data);
		break;

	case CTRL_DEBUG: break;
	case CTRL_MACRO: break;
	}

	mutex_unlock(&data->lock);
	return 0;
}

// NOTE: buf points to an array of PAGE_SIZE (or 4096 bytes on x86)
static ssize_t profile_num_show(struct device* dev, struct device_attribute* attr, char* buf) {
	struct drvdata* data = dev_get_drvdata(dev);
	return snprintf(buf, 4, "%d", data->profile); 
}

static ssize_t profile_num_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len) {
	unsigned long profile;
	int status;
	struct drvdata* data = dev_get_drvdata(dev);

	// Add a null terminator to the input string
	char nbuf[4] = { 0 };
	memcpy(nbuf, buf, (len > 3) ? 3 : len);		// nbuf[3] will always remain 0
	
	// Convert the provided (base 10) string into a number
	status = kstrtoul(nbuf, 10, &profile);
	if (status) { 
		printk(KERN_WARNING "HID Tartarus: Unable to convert sysfs profile value '%s'\n", nbuf);
		return len;
	}

	mutex_lock(&data->lock);
	switch (data->inum) {
		case KBD_INUM: swap_profile_kbd(dev->parent, data, profile); break;
	}
	mutex_unlock(&data->lock);
	
	return len;
}

// Manage the device profile (keymap) itself
// Outputs the map of the currently selected profile
static ssize_t profile_show(struct device* dev, struct device_attribute* attr, char* buf) {
	size_t len = 0;
	struct drvdata* data = dev_get_drvdata(dev);
	struct kbddata* kdata;
	// struct mousedata* mdata;
	u8 profile;

	mutex_lock(&data->lock);
	switch (data->inum) {
	case EXT_INUM: break;
	case KBD_INUM:
		// Keyboard
		profile = data->profile;
		if (!profile) break;		// Profile 0 reserved for "no profile"
		
		len = sizeof(struct profile);
		kdata = data->idata;
		
		memcpy(buf, kdata->maps + profile - 1, len);
		break;

	case MOUSE_INUM:
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

// TODO: Needs testing
static ssize_t profile_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len) {
	struct drvdata* data = dev_get_drvdata(dev);
	struct kbddata* kdata;
	// struct mousedata* mdata;
	struct bind* profile_ptr;
	u8 profile_num;
	u8 bytes = 0;
	
	mutex_lock(&data->lock);
	switch (data->inum) {
	case EXT_INUM: break;
	case KBD_INUM:
		profile_num = data->profile;
		if (!profile_num) break;		// Profile 0 reserved for "no profile"
		
		bytes = (len > sizeof(struct profile)) ? sizeof(struct profile) : len;
		kdata = data->idata;

		profile_ptr = kdata->maps[profile_num - 1].keymap;

		// TODO: If we change the structure of a profile, we want to asser that len is at least long enough
		// 		 for essential metadata information (like which lights to use for example)
		memcpy(profile_ptr, buf, bytes);
		memset(profile_ptr + bytes, 0, sizeof(struct profile) - bytes);
		printk(KERN_INFO "HID Tartarus: Wrote %d bytes to keyboard profile %d\n", bytes, profile_num);
		break;

	case MOUSE_INUM:
		// TODO
		break;
	}

	mutex_unlock(&data->lock);
	return bytes;
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
		printk(KERN_WARNING "HID Tartarus: Ran out of memory while logging device event (inum: %d)\n", inum);
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
struct bind key_event(struct kbddata* kdata, u8 profile_num, int* pstate, u8* event, int len) {
	union keystate state_new = { 0 };
	struct bind action = { 0 };

	int i;
	u8 key = 0;		// Optional intermediate value for my sanity
	u32 state = 0;

	struct profile* map;

	// Check if device is disabled
	if (!profile_num) return action;

	// Determine state change (if any)
	// TODO: Assert that len is 8
	for (i = 2; i < len; ++i) {
		if (!(key = event[i])) break;			// Key-presses always start at 2 and are listed sequentially
		state_new.bytes[key / 8] |= 1 << (key % 8);
	}

	// NOTE: We assume that there will be at most one different key
	for (i = 0; i < 8; ++i) {
		state = state_new.words[i] ^ kdata->state.words[i];

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
		kdata->state.bytes[key / 8] ^= 1 << (key % 8);
		*pstate = !!(state_new.bytes[key / 8] & 1 << (key % 8));
		
	// Event was not a normal key, check modifier keys
	} else {
		//      0000 0100	(alt held)
		// XOR  0000 0110	(shift pressed)
		//	   -----------
		//      0000 0010	(shift)

		key = event[0] ^ kdata->modkey;
		if (!key) return action;

		kdata->modkey ^= key;
		*pstate = !!(event[0] & key);

		// Convert the modifier key bit into a unique key index
		// Resulting values have been verified not to conflict with device values
		// ^^Alt = 0x44 ; Shift = 0x42
		key |= MODKEY_MASK;
	}

	/*/ (DEBUG) Rotate profiles
	if (key == 0x42 && !(*pstate)) {
		action.type = CTRL_PROFILE;
		action.data = profile_num + 1;
		return action;
	} //*/

	// Check if this is a hypershift return (to ignore the usual keybind)
	// TODO: Determine if hinting that this will usually be false improves performance
	if (!(*pstate) && (kdata->shift == key)) {
		// Key index is guaranteed here
		// NOTE: Profile key will be set as active HS key and then released triggering this block
		action.type = CTRL_SHIFT;
		action.data = kdata->prev_profile;		// Shift is ignored if action.data is 0
		kdata->shift = 0;
		// kdata->prev_profile = 0;				// This value is always overridden by a SHIFT or PROFILE event below
		return action;
	}

	// Look up keybind in device profile
	map = kdata->maps + profile_num - 1;
	action = map->keymap[key];

	// Set device data if hypershift or profile swap
	// NOTE: Possible to arrive here with the release of a hypershift key that isn't the active key (crackhead keymap scenario)
	switch (action.type) {
	case CTRL_PROFILE:
		// Profile key release is processed as a shift release, so "shift" to no profile
		profile_num = 0;
		fallthrough;
	case CTRL_SHIFT:
		if (*pstate) {
			kdata->shift = key;						// New HS/profile was pressed
			kdata->prev_profile = profile_num;		// Handle return event
		} else action.type = CTRL_NOP;				// Ignore release 
	}
	
	return action;
}

// Swap keyboard profiles (change profile number and handle currently pressed keys)
// NOTE: This should only be called when wrapped in a device mutex lock!
void swap_profile_kbd(struct device* parent, struct drvdata* data, u8 profile_num) {
	u8* idx;
	u8 pressed;
	u8 profile_num_prev;
	u8 ignore_key;
	struct kbddata* kdata = data->idata;

	struct bind* keymap_old;
	struct bind* keymap_new;
	struct bind key_old;
	struct bind key_new;

	// List of all available tartarus keys
	// TODO: There might be a better way to do this, but this seems the most intiutive
	u8 keys[] = {
		RZKEY_01, RZKEY_02, RZKEY_03, RZKEY_04, RZKEY_05,
		RZKEY_06, RZKEY_07, RZKEY_08, RZKEY_09, RZKEY_10,
		RZKEY_11, RZKEY_12, RZKEY_13, RZKEY_14, RZKEY_15,
		RZKEY_16, RZKEY_17, RZKEY_18, RZKEY_19, RZKEY_20,
		RZKEY_CIRCLE, RZKEY_THMB_L, RZKEY_THMB_U, RZKEY_THMB_R, RZKEY_THMB_D,
		0
	};

	profile_num = profile_num ? (profile_num - 1) % PROFILE_COUNT + 1 : 0;
	if (data->profile == profile_num) return;

	// Update profile state
	// kdata->prev_profile = profile_num ? data->profile : 0;
	profile_num_prev = profile_num ? data->profile : 0;
	data->profile = profile_num;
	ignore_key = kdata->shift;

	// printk(KERN_INFO "Razer Tartarus: Swapping to profile: %d\n", profile_num);		// (DEBUG)

	// Set profile indicator LEDs
	set_profile_led(parent, 0x0C, profile_num & 0x04);		// Red
	set_profile_led(parent, 0x0D, profile_num & 0x02);		// Green
	set_profile_led(parent, 0x0E, profile_num & 0x01);		// Blue

	// Swap keypresses (ignore key available in kdata->shift)
	// Do nothing if device was diabled and subsequently enabled (profile 0 -> profile any)
	if (!profile_num || !profile_num_prev) return;						

	keymap_old = kdata->maps[profile_num_prev - 1].keymap;
	keymap_new = kdata->maps[profile_num - 1].keymap;

	for (idx = keys; *idx != 0; ++idx) {
		// printk(KERN_INFO "Checking key: 0x%02x\n", *idx);		// (DEBUG)
		pressed = kdata->state.bytes[*idx / 8] & 1 << (*idx % 8);
		if (!pressed || *idx == ignore_key) continue;

		key_old = keymap_old[*idx];
		key_new = keymap_new[*idx];
		if (key_old.data == key_new.data) continue;		// This only works because we only handle normal keys below

		// Only handle normal key events (nested profile swaps/macros/etc ignored)
		// NOTE: How this behaves is mostly a personal preference

		// Only send key -> key
		if (key_old.type != CTRL_KEY) continue;
		input_report_key(data->input, key_old.data, 0x00);
		if (key_new.type == CTRL_KEY) input_report_key(data->input, key_new.data, 0x01);

		// Always send all keys variation
		// if (key_old.type == CTRL_KEY) input_report_key(data->input, key_old.data, 0x00);	// Release key
		// if (key_new.type == CTRL_KEY) input_report_key(data->input, key_new.data, 0x01);	// Press key
	}
}


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

	printk(KERN_INFO "HID TARTARUS DEBUG INFORMATION:\n\n\
		\tTransaction ID: 0x%02x\
		\tStatus: %s\n\
		\tCommand ID: 0x%02x\
		\tClass: 0x%02x\n\
		\tSize: 0x%02x (%d)\t\tRemaining Packets: 0x%02x (%d)\n\n\
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
// TODO: Change me!
//     ^^should return a status and pass in a pointer to req and a pointer to out
//	   ^^if out is NULL, don't request the output from the driver
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
		printk(KERN_WARNING "HID Tartarus: No memory for control message request\n");
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
	received = usb_control_msg(tartarus, usb_sndctrlpipe(tartarus, 0), 0x09, 0X21, 0x300, 0x01, request, REPORT_LEN, USB_CTRL_SET_TIMEOUT);
	usleep_range(WAIT_MIN, WAIT_MAX);
	if (received != REPORT_LEN) {
		printk(KERN_WARNING "HID Tartarus: Device data transfer failed\n");
		if (cmd_errno) *cmd_errno = (received < 0) ? received : -EIO;
		goto send_command_exit;
	}

	// Step two - Attempt to get the data out
	// We've prepared an empty buffer, and the device will populate it
	//  0x01 --> HID_REQ_GET_REPORT
	//  0XA1 --> USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN0x00, 0x86, 0x02
	memset(request, 0, sizeof(struct razer_report));
	received = usb_control_msg(tartarus, usb_rcvctrlpipe(tartarus, 0), 0x01, 0XA1, 0x300, 0x01, request, REPORT_LEN, USB_CTRL_SET_TIMEOUT);
	// TODO: Do I need to use usleep_range again?
	if (received != REPORT_LEN) {
		// We've already made the attempt so return what we've got
		printk(KERN_WARNING "HID Tartarus: Invalid device data transfer. (%d bytes != %d bytes)\n", received, REPORT_LEN);
		if (cmd_errno) *cmd_errno = received;
	}

	// We aren't referencing the buffer so we copy it to the stack
	memcpy(&response, request, sizeof(struct razer_report));

send_command_exit:
	kfree(request);
	return response;
}

void set_profile_led_complete(struct urb* ctrl) {
	if (ctrl->status) printk(KERN_WARNING "HID Tartarus: Failed to send control URB\n");

	if (ctrl->context) kfree(ctrl->context);
	usb_free_urb(ctrl);
}

// Asynchronous device control request
// Use this to change profile LEDs
// dev -> Tartarus itself (idev->parent)
void set_profile_led(struct device* dev, u8 led_idx, u8 state) {
	struct usb_interface* intf = to_usb_interface(dev);
	struct usb_device* usbdev = interface_to_usbdev(intf);

	// NOTE: The buffer will be read from direct memory access (DMA) so it is recommended to malloc this field
	struct urb_context* context = kzalloc(sizeof(struct urb_context), GFP_ATOMIC);
	struct usb_ctrlrequest* setup;
	struct razer_report* req;
	struct urb* ctrl;

	if (!context) return;		// TODO: Graceful errors

	// Populate the setup buffer
	setup = &context->setup;
	setup->bRequestType = 0x21;
	setup->bRequest = 0x09;
	setup->wValue = 0x300;
	setup->wIndex = 0x01;
	setup->wLength = REPORT_LEN;

	// Populate the request buffer
	// TODO: Determine if it is possible to set all 3 LEDs at once
	req = &context->req;
	req->tr_id.id = 0xFF;
	req->class = 0x03;
	req->size = 0x03;
	req->data[0] = 0x00;		// TODO: Can be 0 or 1, but unsure what this param does (variable store?)
	req->data[1] = led_idx;		// BLUE -> 0x0E ; GREEN -> 0x0D ; RED -> 0x0C
	req->data[2] = !!state;
	req->cksum = report_checksum(req);

	// TODO: Error handling
	ctrl = usb_alloc_urb(0, GFP_ATOMIC);
	if (!ctrl) {
		kfree(context);
		return;
	}
	
	usb_fill_control_urb(ctrl, usbdev, usb_sndctrlpipe(usbdev, 0), (unsigned char*) setup, req, REPORT_LEN, set_profile_led_complete, context);
	usb_submit_urb(ctrl, GFP_ATOMIC);
}
