#pragma once

#include <stdint.h>
#include <assert.h>
#include "rp2350_otp_ecc.h"

#ifdef __cplusplus
extern "C" {
#endif


void apply_whitelabel_data(void);
bool apply_manufacturing_string(const char* manufacturing_data_string);


#ifdef __cplusplus
}
#endif
