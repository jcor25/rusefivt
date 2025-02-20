#include "pch.h"
#include "hellen_meta.h"

Gpio getCommsLedPin() {
	return H144_LED2_GREEN ;
}

Gpio getRunningLedPin() {
	return H144_LED3_BLUE;
}

Gpio getWarningLedPin() {
	return H144_LED4_YELLOW;
}
