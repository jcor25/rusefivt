

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp
DDEFS += -DEFI_MAIN_RELAY_CONTROL=TRUE

# This board has two tle9104
DDEFS += -DBOARD_TLE9104_COUNT=2


include $(BOARDS_DIR)/hellen/hellen-common100.mk


DDEFS += -DBOOT_BACKDOOR_ENTRY_TIMEOUT_MS=0

DDEFS += $(PRIMARY_COMMUNICATION_PORT_USART2)
