/*/
DEFINES A HARD-CODED DEFAULT MAP
Eventually this will be replaced by config files within a profile system
/*/

/*/ Keymap entry
	Decent list of scan codes: https://flint.cs.yale.edu/cs422/doc/art-of-asm/pdf/APNDXC.PDF

	NOP 		-> 	data is 0
	KEYMAP		->	data is mapped key scancode (see link above)
	HYPERSHIFT	->	data is 0 (swaps to hypershift map)
	MACRO		->	data is index inside macro array (array of pointers to macro types)
	PROFILE		->	data is (probably) index of profile (all files in the same dir, profile1.rz, profile2.rz, ...)
/*/
struct keymap {
	u8 type;		// Event type
	u8 data;		// Respective data
};

static struct keymap default_map [26] = {
	{
		.type = 0x00
	},

	// 01
	{
		.type = 0x01,
		.data = 0x02
	},

	// 02
	{
		.type = 0x01,
		.data = 0x03
	},

	// 03
	{
		.type = 0x01,
		.data = 0x04
	},

	// 04
	{
		.type = 0x01,
		.data = 0x05
	},

	// 05
	{
		.type = 0x01,
		.data = 0x06
	},

	// 06
	{
		.type = 0x01,
		.data = 0x01
	},

	// 07
	{
		.type = 0x01,
		.data = 0x10
	},

	// 08
	{
		.type = 0x01,
		.data = 0x11
	},

	// 09
	{
		.type = 0x01,
		.data = 0x12
	},

	// 10
	{
		.type = 0x01,
		.data = 0x13
	},

	// 11
	{
		.type = 0x01,
		.data = 0x2B
	},

	// 12
	{
		.type = 0x01,
		.data = 0x1E
	},

	// 13
	{
		.type = 0x01,
		.data = 0x1F
	},

	// 14
	{
		.type = 0x01,
		.data = 0x20
	},

	// 15
	{
		.type = 0x01,
		.data = 0x21
	},

	// 16
	{
		.type = 0x01,
		.data = 0x2A
	},

	// 17
	{
		.type = 0x01,
		.data = 0x2C
	},

	// 18
	{
		.type = 0x01,
		.data = 0x2D
	},

	// 19
	{
		.type = 0x01,
		.data = 0x2E
	},

	// 20
	{
		.type = 0x01,
		.data = 0x39
	},
};