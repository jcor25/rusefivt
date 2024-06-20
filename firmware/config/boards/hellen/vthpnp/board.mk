# Combine the related files for a specific platform and MCU.

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp
DDEFS += -DEFI_MAIN_RELAY_CONTROL=TRUE

# Add them all together
DDEFS += -DFIRMWARE_ID=\"vthpnp\"

DDEFS += -DEFI_SOFTWARE_KNOCK=TRUE -DSTM32_ADC_USE_ADC3=TRUE

ONBOARD_MEMS_TYPE=LIS2DH12

DDEFS += -DEFI_CONSOLE_TX_BRAIN_PIN=Gpio::D6 -DEFI_CONSOLE_RX_BRAIN_PIN=Gpio::D5
DDEFS += -DTS_PRIMARY_UxART_PORT=SD2 -DEFI_TS_PRIMARY_IS_SERIAL=TRUE -DSTM32_SERIAL_USE_USART2=TRUE -DSTM32_UART_USE_USART2=FALSE

include $(BOARDS_DIR)/hellen/hellen-common144.mk

DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_VTHPNP

SHORT_BOARD_NAME = vthpnp



