#ifndef __eR63311_PARAM_H__
#define __eR63311_PARAM_H__

#define LDI_ID_REG 0xC1
#define LDI_ID_LEN 1

/*+ Read Panel ID*/
static const unsigned char SEQ_MCAP_OFF[] = {
	0xB0,
	0x04, 0x00
};

static const unsigned char SEQ_MCAP_ON[] = {
	0xB0,
	0x03, 0x00
};

static const unsigned char SEQ_ID_REG[] = {
	0xC1,
	0x00, 0x00
};

static const unsigned char SEQ_SET_ADDR_MODE[] = {
	0x36,
	0xC0, 0x00
};

static const unsigned char SEQ_DISPLAY_CONDITION_SET[] = {
	0xF2,
	0x80, 0x03, 0x0D
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03, 0x00
};

static const unsigned char SEQ_ELVSS_CTRL[] = {
	0xB1,
	0x04, 0x95
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_DISP_ON[] = {
	0x29,
	0x00, 0x00
};

#endif
