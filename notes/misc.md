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