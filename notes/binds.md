```c
	unsigned* keymap;
	unsigned* keymap_hs;

	// Prepare the device keymap
	device_data->hypershift = 0;

	// TODO: parse_keymap(<device_data>, <file>, ...)
	keymap = device_data->keymap;
	memset(keymap, 0, sizeof(unsigned) * USAGE_IDX);

	// Attempt to start communication with the device
	status = hid_parse(dev);		// I think this populates dev->X where X is necessary for hid_hw_start()
	if (status) goto probe_fail;

	status = hid_hw_start(dev, HID_CONNECT_DEFAULT);
	if (status) goto probe_fail;

	// ~~

	probe_fail:
	if (device_data) kfree(device_data);
	printk(KERN_WARNING "Failed to start HID device: Razer Tartarus");
		return status;
```

```c
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
```


```c
// Stop the device 
// if (!(*device_data)) hid_hw_stop(dev);
hid_hw_stop(dev);
```