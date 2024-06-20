//DO NOT EDIT MANUALLY, let automation work hard.

// auto-generated by PinoutLogic.java based on config\boards\hellen\vthpnp\connectors\main.yaml
#include "pch.h"

// see comments at declaration in pin_repository.h
const char * getBoardSpecificPinName(brain_pin_e brainPin) {
	switch(brainPin) {
		case Gpio::A6: return "D4  - SCS/RES1";
		case Gpio::B1: return "D6  - VTPSW/RES2";
		case Gpio::B8: return "A6  - TACH";
		case Gpio::C13: return "A8  - IGN 1";
		case Gpio::C6: return "A9  - IACV/PWM1";
		case Gpio::C7: return "A11 - EGR/PWM2";
		case Gpio::C9: return "A17 - IAB/PWM3";
		case Gpio::D10: return "A2  - INJ 4";
		case Gpio::D11: return "A5  - INJ 3";
		case Gpio::D14: return "A18 - HS2";
		case Gpio::D15: return "A4  - VTS/HS1";
		case Gpio::E12: return "B6  - DIN2";
		case Gpio::E13: return "B5  - AC/DIN1";
		case Gpio::E14: return "B15 - CKP/DIN7";
		case Gpio::E15: return "B11 - CMP/DIN6";
		case Gpio::E2: return "A21 - ICM CTR";
		case Gpio::E3: return "A22 - IGN 4";
		case Gpio::E4: return "A14 - IGN 3";
		case Gpio::E5: return "A10 - IGN 2";
		case Gpio::F3: return "B7  - CLUTCH/DIN3";
		case Gpio::F4: return "B8  - PSSW/DIN4";
		case Gpio::F5: return "B10 - VSS/DIN5";
		case Gpio::F6: return "D2  - Brk/DIN8";
		case Gpio::G2: return "A7  - FP Rly";
		case Gpio::G3: return "A13 - MIL CTR";
		case Gpio::G4: return "A12 - FAN Rly";
		case Gpio::G5: return "A15 - AC Rly";
		case Gpio::G6: return "A16 - Alt CTR";
		case Gpio::G7: return "A1  - INJ 1";
		case Gpio::G8: return "A2  - INJ 2";
		default: return nullptr;
	}
	return nullptr;
}
