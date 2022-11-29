```c
// Core logic of kebind resolution
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
	// This seems to error out some of the mousewheel codes...
	if (!field || !field->hidinput || !usage->type) return -1;
	input = field->hidinput->input;

	// Prune non keyboard types (right now mouse functions pass through)
	// Event types: https://www.kernel.org/doc/html/latest/input/event-codes.html#event-types
	//	Keyboard is type 0x01
	// 	Mouse is type 0x02
	// if (usage->type != 0x01) return 0;	// Disabled for debugging

	// Some testing to figure out codes
	/*if (value)*/

	struct usb_interface* interface = to_usb_interface(dev->dev.parent);

	printk(KERN_INFO "Event Info:  type: 0x%02x  code: 0x%02x  value: 0x%02x  index: 0x%02x  hid: 0x%08x\n", usage->type, usage->code, value, usage->usage_index, usage->hid);
	printk(KERN_INFO "Event Info:  device: 0x%02x\n\n", interface->cur_altsetting->desc.bInterfaceNumber);
	if (usage->usage_index > 0x07) printk("\n");

	return 0;

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
```