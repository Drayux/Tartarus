#include "module.h"			// Module and device defines
#include "keymap.h"			// HARD-CODED DEFAULT PROFILE

// -- DEVICE EVENTS --
// Probe called upon device detection (initalization step)
static int device_probe (struct hid_device* dev, const struct hid_device_id* id) {
	int status;

	struct usb_interface* intf = to_usb_interface(dev->dev.parent);
	struct usb_device* parent = interface_to_usbdev(intf);
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
		memcpy(kdata->maps[0].keymap, debug_keymap, sizeof(struct profile));		
		memcpy(kdata->maps[1].keymap, base_keymap, sizeof(struct profile));
		memcpy(kdata->maps[2].keymap, shift_keymap, sizeof(struct profile));
		//*/
		
		// Create device files
		if((status = device_create_file(&dev->dev, &dev_attr_profile_count))) goto probe_fail;
		if((status = device_create_file(&dev->dev, &dev_attr_profile_num))) goto probe_fail;
		if((status = device_create_file(&dev->dev, &dev_attr_profile))) goto probe_fail;

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
	data->profile = 1;
	data->inum = inum;
	data->idata = idata;
	data->parent = parent;
	
	hid_set_drvdata(dev, data);

	// Begin device communication
	// NOTE: Device must be fully prepared before hid_hw_start() (including dev_drvdata)
	if ((status = hid_parse(dev))) goto probe_fail;
	if ((status = hid_hw_start(dev, HID_CONNECT_DEFAULT))) goto probe_fail;

	// Ensure the device starts with the right profile LED
	if (inum == KBD_INUM) set_profile(data, data->profile);

	// Log success to kernel
	printk(KERN_INFO "HID Tartarus: Successfully bound device driver  Vendor ID: 0x%02x  Product ID: 0x%02x  Interface Num: 0x%02x\n", id->vendor, id->product, inum);
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
static int input_config (struct hid_device* dev, struct hid_input* input) {
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
		// Keyboard (https://elixir.bootlin.com/linux/v6.7/source/drivers/input/input.c#L432)
		// KEYCODES: https://elixir.bootlin.com/linux/v6.7/source/include/uapi/linux/input-event-codes.h#L65
		set_bit(EV_KEY, input_dev->evbit);
		for (int i = 1; i <= 248; ++i) set_bit(i, input_dev->keybit);

		// (DEBUG)
		// printk(KERN_INFO "Device pointers : Input -> %p , Device %p , Parent %p\n", &input->input->dev.parent, dev->dev.parent, dev->dev.parent->parent);
		
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
static void device_disconnect (struct hid_device* dev) {
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
		device_remove_file(&dev->dev, &dev_attr_profile_count);
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
static int handle_event (struct hid_device* dev, struct hid_report* report, u8* raw_event, int raw_event_len) {
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

	// static int ev_num = 0;		// (DEBUG)

	int i;
	int len = 0;
	struct event evlist[KEYLIST_LEN];
	struct drvdata* data = hid_get_drvdata(dev);
	struct kbddata* kdata;

	if (!data) return -1;				// Device not initalized
	if (!data->profile) return 0;		// Device is disabled

	// We use a mutex here because some keys change the device profile
	// As a result, it would be possible to press a key and release a different key	
	mutex_lock(&data->lock);
	// log_event(event, size, (data) ? data->inum : 0xFF); 	// (DEBUG)

	// Build a list of input actions from the event, updating the device state
	switch (data->inum) {
	case KBD_INUM:
		kdata = data->idata;
		len = process_event_kbd(evlist, kdata->keylist, raw_event, raw_event_len);
		for (i = 0; i < len; ++i) resolve_event_kbd(evlist + i, data);
		break;

	case MOUSE_INUM:
		// TODO: Mouse events
		break;
	}

	/*/ (DEBUG)
	++ev_num;
	for (i = 0; i < len; ++i) printk(KERN_INFO "EVENT %d -- Key Action: 0x%02x (%s)\n", 
		ev_num, evlist[i].idx, evlist[i].state ? "DOWN" : "UP"); //*/

	mutex_unlock(&data->lock);
	return 0;
}

// The number of profiles the device was compiled to support
static ssize_t profile_count (struct device* dev, struct device_attribute* attr, char* buf) {
	return snprintf(buf, 4, "%d", PROFILE_COUNT);
}

// NOTE: buf points to an array of PAGE_SIZE (or 4096 bytes on x86)
static ssize_t profile_num_show (struct device* dev, struct device_attribute* attr, char* buf) {
	struct drvdata* data = dev_get_drvdata(dev);
	return snprintf(buf, 4, "%d", data->profile); 
}

static ssize_t profile_num_store (struct device* dev, struct device_attribute* attr, const char* buf, size_t len) {
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

	// Clamp the profile number to acceptable values
	if (profile) profile = (profile - 1) % PROFILE_COUNT + 1;

	mutex_lock(&data->lock);
	switch (data->inum) {
	case KBD_INUM: 
		// Release all (not already ignored) keys
		swap_profile_kbd(data, 0, NULL);
		set_profile(data, profile);
		break;
	}
	mutex_unlock(&data->lock);
	
	return len;
}

// Manage the device profile (keymap) itself
// Outputs the map of the currently selected profile
static ssize_t profile_show (struct device* dev, struct device_attribute* attr, char* buf) {
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
static ssize_t profile_store (struct device* dev, struct device_attribute* attr, const char* buf, size_t len) {
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
		memset((char*) profile_ptr + bytes, 0, sizeof(struct profile) - bytes);
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
void log_event (u8* data, int len_data, u8 inum) {
	int i;
	int j;
	unsigned mask = 0x80;
	// static int num = 0;		// (DEBUG)

	char* bits_str = kzalloc(9 * sizeof(char), GFP_KERNEL);
	char* data_str = kzalloc((10 * len_data + 1) * sizeof(char), GFP_KERNEL);

	// I'm kinda cheating but this won't go into the "production build" of the module
	if (!bits_str || !data_str) {
		printk(KERN_WARNING "HID Tartarus: Ran out of memory while logging device event (inum: %d)\n", inum);
		return;
	}

 	// Bits output version
	for (i = 0; i < len_data; ++i) {
		for (j = 0; j < 8; ++j)
			snprintf(bits_str + j, 2, "%u", !!(data[i] & (mask >> j)));
		snprintf(data_str + (i * 10), 11, "%s%s", bits_str, /*((i + 1) % 8) ? "  " : " \n"*/ "  ");
	}
	printk(KERN_INFO "RAW EVENT:  inum: %d  size: %d  data:\n\t%s\n", inum, len_data, data_str);

	/*/ Hex output version
	for (i = 2; i < len_data; ++i) {
		if (!data[i]) {
			if (i == 2) sprintf(data_str, "EMPTY");
			break;
		}
		snprintf(data_str + (i - 2) * 5, 6, "0x%02x ", data[i]);
	}
	printk(KERN_INFO "EVENT %d: %s\n", ++num, data_str); //*/

	kfree(bits_str);
	kfree(data_str);
}

// Extract key events from the raw event
// Returns the number of elements in the keylist array
// NOTE: If implementing double-binds, that would likely be added here
int process_event_kbd (struct event* evlist, u8* keylist, u8* raw_event, int raw_event_size) {
	u8 key;			// Key from new event
	u8 comp;		// Key from old event
	
	int idx;
	int off = 0;
	int count = 0;

	// Memory safety assertions
	// TODO: Determine expected size conditions
	if (!raw_event_size || !raw_event) return 0;
	if (raw_event_size != KEYLIST_LEN) {
		printk(KERN_WARNING "HID Tartarus: Keyboard raw event has size 0x%02x (different from 0x%02x!)\n", raw_event_size, KEYLIST_LEN);
		raw_event_size = KEYLIST_LEN;
	}

	// Check for changes to the modifier key
	// NOTE: Modifier keys appear to always have their own event
	if ((key = raw_event[0] ^ keylist[0])) {
		keylist[0] = raw_event[0];

		// Convert the modifier key bit into a unique key index
		// Resulting values have been verified not to conflict with device values
		// ^^Alt = 0x44 ; Shift = 0x42
		if (key & MODKEY_SHIFT) evlist[count++] = (struct event) {
			.idx = MODKEY_MASK | MODKEY_SHIFT,
			.state = !!(raw_event[0] & MODKEY_SHIFT)
		};
		
		if (key & MODKEY_ALT) evlist[count++] = (struct event) {
			.idx = MODKEY_MASK | MODKEY_ALT,
			.state = !!(raw_event[0] & MODKEY_ALT)
		};

		return count;
	}

	// Scan old/new keylist for differences in keypresses
	// NOTE: The relative order of the presses should always be consistent
	for (idx = 2; idx < raw_event_size; ++idx) {
		// Brainstorm time
		// New keylist will always be missing a key, or new keys are tacked onto the end
		// Mismatched keys means that key was dropped, unless pointer to top key is no key
		
		key = raw_event[idx];
		comp = (idx + off >= KEYLIST_LEN) ? 0x00 : keylist[idx + off];
		
		if (!key && !comp) break;		// No event data remaining
		if (key == comp) continue;		// Key state is unchanged

		// Key release
		if (comp) {
			evlist[count++] = (struct event) {
				.idx = comp,
				.state = 0x00
			};
			++off;
			--idx;			// key should remain the same value
			continue;
		}

		// Key press
		evlist[count++] = (struct event) {
			.idx = key,
			.state = 0x01
		};
	}

	memcpy(keylist, raw_event, raw_event_size);		// TODO: Should I add sizeof(u8) to this?
	// TODO: If raw_event_size is not always 8, I may need to copy zeros to the remainder of the array (KEYLIST_LEN - raw_event_size)
	return count;
}

// Resolve keyboard action from a key index
// Sets driver interface data relevant to the processing of the mapped action
// NOTE: Does not check for null pointers
void resolve_event_kbd (struct event* ev, struct drvdata* data) {
	/*/ -- Profile functionality overview --
	
		For any key release, check the hypershift "key state" bitmap (TODO: also skip if shift is 0)
		If present, release keys on the revert profile number and update the bitmap (shift_keylist)
		Else release on the original profile
	
		For any key press, send the key on the active profile
		Then, add the key to the bitmap if the current profile and shift profile are the same
		(This is not equivalent to current_profile != shift_profile since a profile swap will not update 'revert')
	
		(Both instances should, and already do, release the key on the device state)

		During a profile swap, all pressed keys will "swap"
		A "swap" releases the original key mapping and presses of the new key mapping, only if both mappings are CTRL_KEY
		All non-'CTRL_KEY -> CTRL_KEY' key indexes will be set in kdata->ignore_keylist (plus profile swap key)
		kdata->shift_keylist will be reset
		Set 'shift' to 0 (or we could enter "permanent" hypershift mode)

		For a hypershift action
		If 'shift' and the target profile are the same, we're big chillin
		Else, perform a release -> ignore of everything in shift_keylist and reset shift_keylist
		(I could also swap to the new hypershift profile but this is a preference case)
		Set 'shift' to the action data (hs profile num) and 'revert' to the current profile num
		Then, set the profile number to the same as 'shift'
		(In most cases, shift_keylist will be empty here)
		(If we have two different hs keys on the same profile, the rare case may occur that we want to hold a key from one hs and enable another hs)
		(This could be resolved by swapping shift_keylist to a list of pairs with key, profile in which it was pressed)
		(In this case, allowing for two different hypershift profiles surpasses the default synapse functionality)
	/*/
	
	struct kbddata* kdata = data->idata;
	u8 base = data->profile;	// NOTE: handle_event() ensures nonzero
	
	struct bind action;
	u8 hs_bit = lookup_profile_kbd (kdata, &action, base, ev->idx, ev->state);
	u8 ig_bit = kdata->ignore_keylist.bytes[ev->idx / 8] & 1 << (ev->idx % 8);

	/*/ (DEBUG)
	printk(KERN_INFO "EVENT: 0x%02x -> 0x%02x, 0x%02x [%s]%s\n", ev->idx, action.type, action.data, 
		ev->state ? "DOWN" : "UP", hs_bit ? " (HS)" : ""); //*/

	// Remove hypershift state bit
	// NOTE: This is for either press state as down sends the non-HS mapping anyway
	//       So this saves us from the "infinite key glitch" if something else broke
	kdata->shift_keylist.bytes[ev->idx / 8] ^= hs_bit;

	// Handle ignored keys
	if (ig_bit) {
		kdata->ignore_keylist.bytes[ev->idx / 8] ^= ig_bit;
		return;
	}

	// Process and report the mapped keybind action accordingly
	switch (action.type) {
	case CTRL_KEY:
		input_report_key(data->input, action.data, ev->state);
		break;

	case CTRL_SHIFT:
		// TODO: Nested hypershift events are weird
		// ^^if target is same as current, then do nothing (maybe, the intuitive solution is that both keys should be released to revert)
		// ^^alternatively we could still set revert so it behaves like a profile swap...
		// New thought: base 1, hs 2, which has swap to 3 -- Intuitive solution is to keep base as 1, release any HS keys in 2
		// Add an int for "pressed hs keys" so that the revert profile is called only when "pressed hs keys" becomes zero
		// ^^OR: Simply do the same as a profile swap and move the old HS key to "ignore keys"

		// -- Hypershift release --
		if (!ev->state) {
			if (kdata->revert) set_profile(data, kdata->revert);
			kdata->revert = 0;
			return;
		}

		// -- Hypershift press --
		// Release all keys in hypershift bitmap when swapping to a different profile
		if (kdata->shift && kdata->shift != action.data)
			swap_profile_kbd(data, 0, &kdata->shift_keylist);

		// Hypershift -> hypershift will not override original profile
		// NOTE: Optional in current implementation to reset 'revert' as only a profile change would change the base map
		if (!kdata->revert) kdata->revert = base;
		
		kdata->shift = action.data;
		set_profile(data, action.data);

		break;

	case CTRL_PROFILE:
		// TODO: Figure out mouse support for this (probably involves linking to the other device data?)
		// NOTE: Only presses should end up here but for the sake of robustness
		if (!ev->state) return;		// Swap to a break if we end up needing post-processing

		// NOTE: Ignore bit gets set within the swap_profile routine since action_release is CTRL_PROFILE
		swap_profile_kbd(data, action.data, NULL);
		set_profile(data, action.data);

		kdata->shift = 0;
		kdata->revert = 0;
		// TODO: We might want to reset the shift bitmap... kdata->shift_keylist = (struct keystate) { 0 };
		// TODO: If adding hs key counter, we would reset that here
		
		break;

	case CTRL_DEBUG: break;
	}

	// Set hypershift state bit
	if (ev->state && data->profile == kdata->shift) kdata->shift_keylist.bytes[ev->idx / 8] |= 1 << (ev->idx % 8);
}

// Sets ev to the action we should use with respect to the current state
// Returns the hypershift bit mask for key RELEASE
u8 lookup_profile_kbd (struct kbddata* kdata, struct bind* action, u8 base, u8 key, u8 pstate) {
	struct profile* map = kdata->maps;
	u8 idx = base;
	u8 hs_bit = 0;

	/*/ More detailed breakdown of the below conditional
	if (!pstate) {
		if (hypershift bit && shift) {
			idx = kdata->shift;
			// if not shift, then idx = base too
		} else {
			if (hypershift mode) {
				if (!revert) < do nothing >;
				idx = revert;
			} else {
				idx = base;
			}
		}
	} //*/

	if (!pstate) {
		hs_bit = kdata->shift_keylist.bytes[key / 8] & 1 << (key % 8); // & ~(!kdata->shift);
		if (hs_bit && kdata->shift) idx = kdata->shift;
		else if (base == kdata->shift) idx = kdata->revert;
	}

	printk(KERN_INFO "Base: %d, hs_bit: 0x%02x, shift: %d, revert: %d --> idx: %d\n", base, hs_bit, kdata->shift, kdata->revert, idx);

	// Idx should be at least 1 ; 0 means to take no action
	if (!idx) {
		*action = (struct bind) { 0 };
		return 0;
	}

	map += idx - 1;
	*action = map->keymap[key];
	return hs_bit;
}

// Swap keypresses across profiles
// profile -> Profile number to change to ; 0 -> release keys only
// whitelist -> If not NULL, bitmap of keys to exclusively consider
// TODO: Because of multiple actions in one event, it is currently possible to have a rare double-press when the 
//       "pressed but not processed" key is released, pressed in the new profile, and then pressed in the new profile for real
void swap_profile_kbd (struct drvdata* data, u8 profile, struct keystate* whitelist) {
	struct kbddata* kdata = data->idata;
	
	u8* keylist = kdata->keylist;	// Do not modify!
	struct keystate* shift_kl = &kdata->shift_keylist;
	struct keystate* ignore_kl = &kdata->ignore_keylist;

	struct bind action_press;
	struct bind action_release;
	
	u8 key;
	u8 hs_bit;
	int i;

	for (i = 2; i < KEYLIST_LEN; ++i) {
		if (!(key = keylist[i])) return;
		if (ignore_kl->bytes[key / 8] & 1 << (key % 8)) continue;
		if (whitelist && !(whitelist->bytes[key / 8] & 1 << (key % 8))) continue;

		// NOTE: hs_bit used to unset a bit from the hypershift bitmap
		// Currently, we could just replace the entire map at the end of this operation instead
		// The profile swap key (even shift -> profile) will not be set in the HS bitmap when shift is set to 0
		hs_bit = lookup_profile_kbd(kdata, &action_release, data->profile, key, 0);
		lookup_profile_kbd(kdata, &action_press, profile, key, 0);

		switch (action_release.type) {
		case CTRL_KEY:
			if (action_press.type == CTRL_KEY && action_release.data == action_press.data) break;

			// Send up of old and down of new (key -> key only)
			// NOTE: action_press becomes a CTRL_NOP when profile is 0
			input_report_key(data->input, action_release.data, 0);
			if (action_press.type == CTRL_KEY)	{
				input_report_key(data->input, action_press.data, 1);
				break;
			}
			
			fallthrough;
		default:
			// Set the ignore bit
			ignore_kl->bytes[key / 8] |= 1 << (key % 8);
		}

		// Update the hypershift bitmap (TODO: Might be unnecessary since we set the ignore bit)
		shift_kl->bytes[key / 8] ^= hs_bit;
	}
}

// Set device profile number and lights
void set_profile (struct drvdata* data, u8 profile) {
	// TODO: Only change LEDs if KBD_INUM
	set_profile_led(data, 0x0C, profile & 0x04);		// Red
	set_profile_led(data, 0x0D, profile & 0x02);		// Green
	set_profile_led(data, 0x0E, profile & 0x01);		// Blue

	data->profile = profile;
}

// Swap keyboard profiles (change profile number and handle currently pressed keys)
/*/ NOTE: This should only be called when wrapped in a device mutex lock!
void swap_profile_kbd_old (struct device* parent, struct drvdata* data, u8 profile_num) {
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
	// set_profile_led(parent, 0x0C, profile_num & 0x04);		// Red
	// set_profile_led(parent, 0x0D, profile_num & 0x02);		// Green
	// set_profile_led(parent, 0x0E, profile_num & 0x01);		// Blue

	// Swap keypresses (ignore key available in kdata->shift)
	// Do nothing if device was diabled and subsequently enabled (profile 0 -> profile any)
	if (true || !profile_num || !profile_num_prev) return;		// (DEBUG -> true clause)

	keymap_old = kdata->maps[profile_num_prev - 1].keymap;
	keymap_new = kdata->maps[profile_num - 1].keymap;

	for (idx = keys; *idx != 0; ++idx) {
		printk(KERN_INFO "TODO: Checking key: 0x%02x\n", *idx);		// (DEBUG)
		pressed = 0; // kdata->state.bytes[*idx / 8] & 1 << (*idx % 8);
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
} //*/


// -- DEVICE COMMANDS --
// Log the a razer report struct to the kernel (for debugging)
void log_report (struct razer_report* report) {
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
unsigned char report_checksum (struct razer_report* report) {
	unsigned char ck = 0;
	unsigned char* bytes = (unsigned char*) report;

	for (int i = 2; i < 88; i++) ck ^= bytes[i];
	return ck;
}

// Create a razer report data type
// Specifically for use in requesting data from the device
// (Modified from OpenRazer)
struct razer_report init_report (unsigned char class, unsigned char id, unsigned char size) {
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
struct razer_report send_command (struct device* dev, struct razer_report* req, int* cmd_errno) {
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
	usleep_range(600, 800);
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

// Asynchronous device control request
// Use this to change profile LEDs
// dev -> Tartarus itself (idev->parent)
void set_profile_led (struct drvdata* data, u8 led_idx, u8 state) {
	struct usb_device* usbdev = data->parent;

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

	ctrl = usb_alloc_urb(0, GFP_ATOMIC);
	if (!ctrl) {
		kfree(context);
		return;					// TODO: Graceful errors
	}
	
	usb_fill_control_urb(ctrl, usbdev, usb_sndctrlpipe(usbdev, 0), (unsigned char*) setup, req, REPORT_LEN, set_profile_led_complete, context);
	usb_submit_urb(ctrl, GFP_ATOMIC);
}

void set_profile_led_complete (struct urb* ctrl) {
	if (ctrl->status) printk(KERN_WARNING "HID Tartarus: Failed to send control URB\n");

	if (ctrl->context) kfree(ctrl->context);
	usb_free_urb(ctrl);
}
