#ifndef PIM_INIT_STATE_H
#define PIM_INIT_STATE_H


typedef enum {
	SINGLE_BANK,
	ALL_BANK,
	PIM_ALL_BANK
} pim_bank_mode_t;


int set_bank_mode(pim_bank_mode_t);

int set_kernel(void);


#endif