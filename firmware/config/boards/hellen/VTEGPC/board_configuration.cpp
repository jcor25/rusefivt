#include "pch.h"
#include "hellen_meta.h"
#include "defaults.h"
#include "smart_gpio.h"
#include "drivers/gpio/tle9104.h"
#include "pca_board_id.h" // bb i2c board id, works via __weak__ magic

Gpio getCommsLedPin() {
	// this board has no comms led
	return Gpio::Unassigned;
}

Gpio getWarningLedPin() {
	// this board has no warning led
	return Gpio::Unassigned;
}

Gpio getRunningLedPin() {
	// this board has no running led
	return Gpio::Unassigned;
}

static void setInjectorPins() {
	engineConfiguration->injectionPins[0] = Gpio::TLE9104_0_OUT_0;
	engineConfiguration->injectionPins[1] = Gpio::TLE9104_0_OUT_1;
	engineConfiguration->injectionPins[2] = Gpio::TLE9104_0_OUT_2;
	engineConfiguration->injectionPins[3] = Gpio::TLE9104_0_OUT_3;
	engineConfiguration->injectionPins[4] = Gpio::TLE9104_1_OUT_0;
	engineConfiguration->injectionPins[5] = Gpio::TLE9104_1_OUT_1;
	engineConfiguration->injectionPins[6] = Gpio::TLE9104_1_OUT_2;
	engineConfiguration->injectionPins[7] = Gpio::TLE9104_1_OUT_3;
}


static void setupDefaultSensorInputs() {

	engineConfiguration->tps1_1AdcChannel = EFI_ADC_6;
	engineConfiguration->map.sensor.hwChannel = EFI_ADC_7; // On-board MAP

	engineConfiguration->boardUseTempPullUp = true;
	engineConfiguration->clt.adcChannel = EFI_ADC_8;
	engineConfiguration->iat.adcChannel = EFI_ADC_9;

	
}

void setBoardConfigOverrides() {
	/* Force PWR_EN as TLE9104s are powered from +5VA */
	setHellenMegaEnPin();

	setHellenEnPin(A9);


	setHellenVbatt();
	setHellenCan();
	setDefaultHellenAtPullUps();
	setHellenCan();

	/* Two TLE9104 */
	enableHellenSpi2();
}

void setBoardDefaultConfiguration() {
	setInjectorPins();
	setupDefaultSensorInputs();

	engineConfiguration->canTxPin = Gpio::MM100_CAN_TX;
	engineConfiguration->canRxPin = Gpio::MM100_CAN_RX;

	engineConfiguration->can2TxPin = Gpio::B5;
	engineConfiguration->can2RxPin = Gpio::B6;

	engineConfiguration->etbIo[0].directionPin1 = Gpio::E2;
	engineConfiguration->etbIo[0].directionPin2 = Gpio::E5;
	engineConfiguration->etbIo[0].disablePin = Gpio::E3;
	engineConfiguration->etb_use_two_wires = true;



}

/*
 * RESET and EN signals of both TLE9104 are driven by same gpios,
 * Do not allow TLE driver to drive this pins as it will reset
 * first chip while initing second.
 * Set pins to proper once.
 * TODO: improve?
 */
static const tle9104_config tle9104_cfg[BOARD_TLE9104_COUNT] = {
	{
		.spi_bus = &SPID2,
		.spi_config = {
			.circular = false,
			.end_cb = NULL,
			.ssport = GPIOA,
			.sspad = 8,
			.cr1 =
				SPI_CR1_16BIT_MODE |
				SPI_CR1_SSM |
				SPI_CR1_SSI |
				((3 << SPI_CR1_BR_Pos) & SPI_CR1_BR) |	// div = 16
				SPI_CR1_MSTR |
				SPI_CR1_CPHA |
				0,
			.cr2 = SPI_CR2_16BIT_MODE
		},
		.direct_io = {
			{ .port = GPIOC, .pad = 6 },
			{ .port = GPIOC, .pad = 8 },
			{ .port = GPIOC, .pad = 7 },
			{ .port = GPIOC, .pad = 9 }
		},
		.resn = Gpio::Unassigned, //Gpio::B14,
		.en   = Gpio::Unassigned //Gpio::B15
	},
	{
		.spi_bus = &SPID2,
		.spi_config = {
			.circular = false,
			.end_cb = NULL,
			.ssport = GPIOB,
			.sspad = 12,
			.cr1 =
				SPI_CR1_16BIT_MODE |
				SPI_CR1_SSM |
				SPI_CR1_SSI |
				((3 << SPI_CR1_BR_Pos) & SPI_CR1_BR) |	// div = 16
				SPI_CR1_MSTR |
				SPI_CR1_CPHA |
				0,
			.cr2 = SPI_CR2_16BIT_MODE
		},
		.direct_io = {
			{ .port = GPIOD, .pad = 10 }, 
			{ .port = GPIOD, .pad = 12 }, 
			{ .port = GPIOD, .pad = 11 }, 
			{ .port = GPIOD, .pad = 13 } 
		},
		.resn = Gpio::Unassigned, //Gpio::D15,
		.en   = Gpio::Unassigned //Gpio::D14
	}
};

static void board_init_ext_gpios() {
  {
    /* Waste of RAM, switch to palSetPadMode() and palSetPort() */
    static OutputPin TleCs0;
	  TleCs0.initPin("TLE9104 CS0", Gpio::A8);
	  TleCs0.setValue(1);
	}
	{
    static OutputPin TleCs1;
	  TleCs1.initPin("TLE9104 CS1", Gpio::B12);
	  TleCs1.setValue(1);
	}

	{
	  static OutputPin TleReset;
	  TleReset.initPin("TLE9104 Reset", Gpio::D15);
	  TleReset.setValue(1);

	  static OutputPin TleEn;
	  TleEn.initPin("TLE9104 En", Gpio::D14);
	  TleEn.setValue(1);
	}

  initAll9104(tle9104_cfg);
}

/**
 * @brief Board-specific initialization code.
 * @todo  Add your board-specific code, if any.
 */


static Gpio OUTPUTS[] = {
	Gpio::TLE9104_0_OUT_3, // 1A - Injector 4
	Gpio::TLE9104_0_OUT_2, // 2A - Injector 3
	Gpio::TLE9104_0_OUT_1, // 3A - Injector 2
	Gpio::TLE9104_0_OUT_0, // 4A - Injector 1
	Gpio::TLE9104_1_OUT_1, // 1A - Injector 8
	Gpio::TLE9104_1_OUT_2, // 1A - Injector 7
	Gpio::TLE9104_1_OUT_3, // 1A - Injector 6
	Gpio::TLE9104_1_OUT_0, // 1A - Injector 5
	Gpio::A15,	   // 1A - GPIO1
	Gpio::C10,	   // 1A - GPIO2

};

int getBoardMetaOutputsCount() {
    return efi::size(OUTPUTS);
}

Gpio* getBoardMetaOutputs() {
    return OUTPUTS;
}
