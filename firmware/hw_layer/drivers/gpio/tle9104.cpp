#include "pch.h"
#include "gpio/gpio_ext.h"
#include "gpio/tle9104.h"

#if defined(BOARD_TLE9104_COUNT) && BOARD_TLE9104_COUNT > 0

/*
 * TODO list:
 * - support driving outputs over SPI
 * - count communication errors, POR events, etc
 * - support WD functionality
 * - implement direct-io mapping test. Check if user provides correct direct_io data
 */

/*==========================================================================*/
/* Driver local definitions.												*/
/*==========================================================================*/

#define DRIVER_NAME					"tle9104"

/* TODO: aling with WD settings */
#define TLE9104_POLL_INTERVAL_MS	100

static bool drv_task_ready = false;

typedef enum {
	TLE9104_DISABLED = 0,
	TLE9104_WAIT_INIT,
	TLE9104_READY,
	TLE9104_FAILED
} tle9104_drv_state;

/* IN MOSI and MISO a read is defined with a s0 and a write is defined with a 1. */
#define TLE9104_WR_REG(addr, val)	(BIT(15) | (((addr) & 0x0f) << 8) | ((val) & 0xff))
#define TLE9104_RD_REG(addr)		(((addr) & 0x0f) << 8)

#define TLE9104_GET_VAL(rx)			((rx) & 0xff)
#define TLE9104_GET_ADDR(rx)		(((rx) >> 8) & 0x0f)

#define TLE9104_PARITY_MASK(val)	((val) & (~BIT(14)))

#define TLE9104_FAULT_GLOBAL		BIT(12)
#define TLE9104_FAULT_COMM			BIT(13)

#define TLE9104_REG_CTRL			0x00
#define TLE9104_REG_CFG				0x01
#define TLE9104_REG_OFF_DIAG_CFG	0x02
#define TLE9104_REG_ON_DIAG_CFG		0x03
#define TLE9104_REG_DIAG_OUT_1_2_ON	0x04
#define TLE9104_REG_DIAG_OUT_3_4_ON	0x05
#define TLE9104_REG_DIAG_OFF		0x06
#define TLE9104_REG_GLOBAL_STATUS	0x07
#define TLE9104_REG_ICVID			0x08

/*==========================================================================*/
/* Driver exported variables.												*/
/*==========================================================================*/

/*==========================================================================*/
/* Driver local variables and types.										*/
/*==========================================================================*/

struct Tle9104 : public GpioChip {
	int init() override;

	int writePad(size_t pin, int value) override;
	brain_pin_diag_e getDiag(size_t pin) override;
	void debug() override;

	int updateDiagState();
	int updateStatus();

	const tle9104_config* cfg;
	tle9104_drv_state drv_state;
private:
	int spi_validate(uint16_t rx);
	int spi_rw_array(const uint16_t *tx, uint16_t *rx, int n);
	int spi_rw(uint16_t tx, uint16_t *rx);
	int read_reg(uint8_t addr, uint8_t *val);
	int write_reg(uint8_t addr, uint8_t val);

	int chip_init();

	OutputPin m_en;
	OutputPin m_resn;

	/* diagnostic registers */
	uint8_t diag_off;
	uint8_t diag_on12;
	uint8_t diag_on34;

	/* statistic */
	int por_cnt;
	int wdr_cnt;
	int init_req_cnt;
	int fault_cnt;
	int fault_comm_cnt;
	int spi_address_err_cnt;
	int spi_cnt;
	int spi_parity_err_cnt;
};

static Tle9104 chips[BOARD_TLE9104_COUNT];

/*==========================================================================*/
/* Driver local functions.													*/
/*==========================================================================*/

static bool parityBit(uint16_t val) {
	// (1 + number of bits set) mod 2 = parity bit
	int count = 1 + __builtin_popcount(val);

	return count % 2;

#if 0
	while (val != 0) {
		if (val & 0x01) {
			count++;
		}

		val = val >> 1;
	}

	return (count % 2) == 1;
#endif
}

int Tle9104::spi_validate(uint16_t rx)
{
	/* with parity bit included */
	bool parityOk = !parityBit(rx);
	if (!parityOk) {
		spi_parity_err_cnt++;
		return -1;
	}

	if (rx & TLE9104_FAULT_GLOBAL) {
		fault_cnt++;
	}

	if (rx & TLE9104_FAULT_COMM) {
		fault_comm_cnt++;
	}

	return 0;
}

int Tle9104::spi_rw(uint16_t tx, uint16_t *rx) {
	SPIDriver *spi = cfg->spi_bus;

	spi_cnt++;

	// set the parity bit appropriately
	tx |= parityBit(tx) << 14;

	/* Acquire ownership of the bus. */
	spiAcquireBus(spi);
	/* Setup transfer parameters. */
	spiStart(spi, &cfg->spi_config);
	/* Slave Select assertion. */
	spiSelect(spi);
	/* Atomic transfer operations. */
	uint16_t rxd = spiPolledExchange(spi, tx);
	/* Slave Select de-assertion. */
	spiUnselect(spi);
	/* Ownership release. */
	spiReleaseBus(spi);

	/* with parity bit included */
	int ret = spi_validate(rxd);
	if (ret < 0) {
		return ret;
	}

	/* TODO: check Fault Global and Fault Communication flags */

	// return data
	if (rx) {
		*rx = rxd;
	}

	return 0;
}

int Tle9104::spi_rw_array(const uint16_t *tx, uint16_t *rx, int n)
{
	int ret = 0;
	SPIDriver *spi = cfg->spi_bus;

	if ((n <= 0) || (tx == nullptr)) {
		return -2;
	}

	/* Acquire ownership of the bus. */
	spiAcquireBus(spi);
	/* Setup transfer parameters. */
	spiStart(spi, &cfg->spi_config);

	for (int i = 0; i < n; i++) {
		spi_cnt++;

		/* Slave Select assertion. */
		spiSelect(spi);
		/* data transfer */
		uint16_t rxdata = spiPolledExchange(spi, tx[i]);

		if (rx)
			rx[i] = rxdata;
		/* Slave Select de-assertion. */
		spiUnselect(spi);

		/* validate reply */
		ret = spi_validate(rxdata);
		if (ret < 0)
			break;

		if (i >= 1) {
			/* validate that we received correct answer to previous tx */
			if (TLE9104_GET_ADDR(tx[i - 1] != TLE9104_GET_ADDR(rxdata))) {
				spi_address_err_cnt++;
				ret = -2;
				break;
			}
		}
	}
	/* Ownership release. */
	spiReleaseBus(spi);

	/* no errors for now */
	return ret;
}

int Tle9104::read_reg(uint8_t addr, uint8_t *val) {
	int ret;

	ret = spi_rw(TLE9104_RD_REG(addr), nullptr);
	if (ret) {
		return ret;
	}

	uint16_t rxd;
	ret = spi_rw(TLE9104_RD_REG(addr), &rxd);
	if (ret) {
		return ret;
	}

	if (val) {
		*val = TLE9104_GET_VAL(rxd);
	}

	return 0;
}

int Tle9104::write_reg(uint8_t addr, uint8_t val) {
	// R/W bit is 1 for write
	return spi_rw(TLE9104_WR_REG(addr, val), nullptr);
}

int Tle9104::chip_init() {
	int ret;

	init_req_cnt++;

	// disable comms watchdog, enable direct drive on all 4 channels
	// TODO: should we enable comms watchdog?
	ret = write_reg(TLE9104_REG_CFG, 0x0F);
	if (ret) {
		return ret;
	}

	// clear any suprious diag states from startup: first call resets, second reads true state
	ret = updateDiagState();
	if (ret) {
		return ret;
	}
	ret = updateDiagState();
	if (ret) {
		return ret;
	}

	// set output enable, clear all other flags
	ret = write_reg(TLE9104_REG_GLOBAL_STATUS, BIT(7));
	if (ret) {
		return ret;
	}

	drv_state = TLE9104_READY;

	return 0;
}

/*==========================================================================*/
/* Driver thread.															*/
/*==========================================================================*/

SEMAPHORE_DECL(tle9104_wake, 10 /* or BOARD_TLE9104_COUNT ? */);
static THD_WORKING_AREA(tle9104_thread_1_wa, 256);

static THD_FUNCTION(tle9104_driver_thread, p)
{
	int i;
	msg_t msg;

	(void)p;

	chRegSetThreadName(DRIVER_NAME);

	while(1) {
		msg = chSemWaitTimeout(&tle9104_wake, TIME_MS2I(TLE9104_POLL_INTERVAL_MS));

		/* should we care about msg == MSG_TIMEOUT? */
		(void)msg;

		for (i = 0; i < BOARD_TLE9104_COUNT; i++) {
			int ret;
			Tle9104& chip = chips[i];

			if (!chip.cfg ||
				(chip.drv_state == TLE9104_DISABLED) ||
				(chip.drv_state == TLE9104_FAILED))
				continue;

			ret = chip.updateDiagState();
			if (ret) {
				/* set state to TLE6240_FAILED? */
			}
			ret = chip.updateStatus();
			if (ret) {
				/* set state to TLE6240_FAILED? */
			}
		}
	}
}

/*==========================================================================*/
/* Driver exported functions.												*/
/*==========================================================================*/

int Tle9104::writePad(size_t pin, int value) {
	// Inverted since TLE9104 is active low (low level to turn on output)
	if (value) {
		palClearPad(cfg->direct_io[pin].port, cfg->direct_io[pin].pad);
	} else {
		palSetPad(cfg->direct_io[pin].port, cfg->direct_io[pin].pad);
	}

	return 0;
}

int Tle9104::updateDiagState() {
	int ret;

	/* TODO: implement and use spi_rw_array() */
	ret = read_reg(TLE9104_REG_DIAG_OUT_1_2_ON, &diag_on12);
	if (ret) {
		return ret;
	}
	ret = read_reg(TLE9104_REG_DIAG_OUT_3_4_ON, &diag_on34);
	if (ret) {
		return ret;
	}
	ret = read_reg(TLE9104_REG_DIAG_OFF, &diag_off);
	if (ret) {
		return ret;
	}

	// clear diag states
	ret = write_reg(TLE9104_REG_DIAG_OUT_1_2_ON, 0);
	if (ret) {
		return ret;
	}
	ret = write_reg(TLE9104_REG_DIAG_OUT_3_4_ON, 0);
	if (ret) {
		return ret;
	}
	ret = write_reg(TLE9104_REG_DIAG_OFF, 0);
	if (ret) {
		return ret;
	}

	return 0;
}

int Tle9104::updateStatus() {
	int ret;

	uint8_t status;
	ret = read_reg(TLE9104_REG_GLOBAL_STATUS, &status);
	if (ret) {
		return ret;
	}

	/* Device was reset since last cleared */
	if (status & BIT(0)) {
		por_cnt++;
		need_init = true;
	}
	/* Communication watchdog timeout occurred */
	if (status & BIT(2)) {
		wdr_cnt++;
		need_init = true;
	}

	if (need_init) {
		ret = chip_init();
		if (ret == 0) {
			need_init = false;
		}
	}

	return 0;
}

brain_pin_diag_e Tle9104::getDiag(size_t pin) {
	uint8_t off_diag;
	uint8_t on_diag;

	switch (pin) {
		case 0:
			on_diag = diag_on12;
			off_diag = diag_off;
			break;
		case 1:
			on_diag = diag_on12 >> 3;
			off_diag = diag_off >> 2;
			break;
		case 2:
			on_diag = diag_on34;
			off_diag = diag_off >> 4;
			break;
		case 3:
			on_diag = diag_on34 >> 3;
			off_diag = diag_off >> 6;
			break;
		default:
			return PIN_UNKNOWN;
	}

	// on diag has 3 bits
	on_diag = on_diag & 0x7;
	// of diag has 2 bits
	off_diag = off_diag & 0x3;

	int result = 0;

	// Decode on-state faults
	switch (on_diag) {
		case 2:
			result |= PIN_SHORT_TO_BAT;
			break;
		case 3:
			// overtemp and overcurrent
			result |= PIN_DRIVER_OVERTEMP;
			[[fallthrough]];
		case 4:
			result |= PIN_OVERLOAD;
			break;
		case 5:
			result |= PIN_DRIVER_OVERTEMP;
			break;
	}

	// Decode off-state faults
	switch (off_diag) {
		case 2:
			result |= PIN_OPEN;
			break;
		case 3:
			result |= PIN_SHORT_TO_GND;
			break;
	}

	return (brain_pin_diag_e)result;
}

void Tle9104::debug() {
	efiPrintf("spi transfers %d with patiry error %d with wrong address %d communication fault counter %d\n",
		spi_cnt, spi_parity_err_cnt, spi_address_err_cnt, fault_comm_cnt);
	efiPrintf("fault counter %d communication fault counter %d\n",
		fault_cnt, fault_comm_cnt);
	efiPrintf("POR counter %d reinit counter %d WD counter %d\n",
		por_cnt, init_req_cnt, wdr_cnt);
}

int Tle9104::init() {
	int ret;

	m_resn.initPin(DRIVER_NAME " RESN", cfg->resn);
	m_en.initPin(DRIVER_NAME " EN", cfg->en);

	// disable outputs
	m_en.setValue(false);
	// Reset the chip
	m_resn.setValue(false);

	/* TODO: ensure all direct_io pins valid, otherwise support manipulationg output states over SPI */
	for (int i = 0; i < 4; i++) {
		gpio_pin_markUsed(cfg->direct_io[i].port, cfg->direct_io[i].pad, DRIVER_NAME " Direct IO");
		palSetPadMode(cfg->direct_io[i].port, cfg->direct_io[i].pad, PAL_MODE_OUTPUT_PUSHPULL);

		// Ensure all outputs are off
		writePad(i, false);
	}

	if (isBrainPinValid(cfg->resn)) {
		chThdSleepMilliseconds(1);
		m_resn.setValue(true);
		chThdSleepMilliseconds(1);
	}

	// read ID register
	uint8_t id;
	ret = read_reg(TLE9104_REG_ICVID, &id);

	if (ret) {
		return ret;
	}
	// No chip detected if ID is wrong
	if (id != 0xB1) {
		return -1;
	}

	ret = chip_init();
	if (ret) {
		return ret;
	}

	// Set hardware enable
	m_en.setValue(true);

	if (!drv_task_ready) {
		chThdCreateStatic(tle9104_thread_1_wa, sizeof(tle9104_thread_1_wa),
						  PRIO_GPIOCHIP, tle9104_driver_thread, nullptr);
		drv_task_ready = true;
	}

	return 0;
}

int tle9104_add(Gpio base, int index, const tle9104_config* cfg) {
	Tle9104& chip = chips[index];

	/* already added? */
	if (chip.cfg != NULL) {
		return -1;
	}

	chip.cfg = cfg;
	chip.drv_state = TLE9104_WAIT_INIT;

	return gpiochip_register(base, DRIVER_NAME, chip, 4);
}

#else // BOARD_TLE9104_COUNT > 0

int tle9104_add(Gpio, int, const tle9104_config*) {
	return -1;
}

#endif // BOARD_TLE9104_COUNT
