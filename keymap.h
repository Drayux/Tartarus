/*/
	// Human readable version
	[0x1E]	[0x1F]	[0x20] 	[0x21]	[0x22]
	 0x02    0x03    0x04    0x05    0x06

	[0x2B]	[0x14]	[0x1A]	[0x08]	[0x15]
	 0x01    0x10    0x11    0x12    0x13

	[0x39]	[0x04]	[0x16]	[0x07]	[0x09]
	 0x2B    0x1E    0x1F    0x20    0x21

	[0x42]	[0x1D]	[0x1B]	[0x06]	[0x2C]
	 0x2A    0x2C    0x2D    0x2E    0x39

	// Circle, arrow L, arrow U, arrow R, arrow D
	[0x44]	[0x50]	[0x52]	[0x4F]	[0x51]
	 (HS)    0x4B    0x48    0x4D    0x50
/*/

const char default_keymap[0x200] = {
	//   0 (0x00)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x1E,  0x00, 0x00,  0x01, 0x2E,  0x01, 0x20,  
	0x01, 0x12,  0x01, 0x21,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x10,  0x01, 0x13,  0x01, 0x1F,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x01, 0x11,  0x01, 0x2D,  0x00, 0x00,  0x01, 0x2C,  0x01, 0x02,  0x01, 0x03,  

	//  32 (0x20)
	0x01, 0x04,  0x01, 0x05,  0x01, 0x06,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x01,  0x01, 0x39,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x01, 0x2B,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	//  64 (0x40)
	0x00, 0x00,  0x00, 0x00,  0x01, 0x2A,  0x00, 0x00,  0x02, 0x02,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x4D,  
	0x01, 0x4B,  0x01, 0x50,  0x01, 0x48,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,

	//  96 (0x60)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 128 (0x80)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 160 (0xA0)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 192 (0xC0)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 224 (0xE0)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
};

const char default_keymap_shift[0x200] = {
	//   0 (0x00)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x1E,  0x00, 0x00,  0x01, 0x2E,  0x01, 0x20,  
	0x01, 0x12,  0x01, 0x21,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x10,  0x01, 0x13,  0x01, 0x1F,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x01, 0x11,  0x01, 0x2D,  0x00, 0x00,  0x01, 0x2C,  0x01, 0x07,  0x01, 0x08,  

	//  32 (0x20)
	0x01, 0x09,  0x01, 0x0A,  0x01, 0x0B,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x01,  0x01, 0x39,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x01, 0x2B,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	//  64 (0x40)
	0x00, 0x00,  0x00, 0x00,  0x01, 0x2A,  0x00, 0x00,  0x02, 0x02,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x01, 0x4D,  
	0x01, 0x4B,  0x01, 0x50,  0x01, 0x48,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,

	//  96 (0x60)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 128 (0x80)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 160 (0xA0)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 192 (0xC0)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  

	// 224 (0xE0)
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
	0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  
};