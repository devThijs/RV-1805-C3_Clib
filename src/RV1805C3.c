/*
	Arduino Library for RV-1805-C3
	
	Copyright (c) 2018 Macro Yau

	https://github.com/MacroYau/RV-1805-C3-Arduino-Library
*/

#include "RV1805C3.h"
#include "i2c_simple_master.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>



bool RV1805C3_init() {
	// Wire.begin() should be called in the application code in advance
	

	// Checks part number
	uint8_t partNumber[2] = {0, 0};
	readBytesFromRegisters(REG_ID0, partNumber, 2);

	if (partNumber[0] == PART_NUMBER_MSB && partNumber[1] == PART_NUMBER_LSB) {
		RV1805C3_enableOscillatorSwitching();
		RV1805C3_reduceLeakage();
		return true;
	} else {
		return false;
	}
}

void RV1805C3_reset() {
	writeByteToRegister(REG_CONFIG_KEY, CONFKEY_RESET);
}

void RV1805C3_enableCrystalOscillator() {
	uint8_t value = readRegister(REG_OSC_CONTROL);
	value &= 0b00011111;
	writeByteToRegister(REG_CONFIG_KEY, CONFKEY_OSC_CONTROL);
	writeByteToRegister(REG_OSC_CONTROL, value);
}

void RV1805C3_disableCrystalOscillator() {
	uint8_t value = readRegister(REG_OSC_CONTROL);
	value &= 0b00011111;
	value |= (1 << 7); // Uses RC oscillator all the time to minimize power usage
	value |= (0b11 << 5); // Enables autocalibration every 512 seconds (22 nA consumption)
	writeByteToRegister(REG_CONFIG_KEY, 0xA1);
	writeByteToRegister(REG_OSC_CONTROL, value);
}

void RV1805C3_enableOscillatorSwitching() {
	uint8_t value = readRegister(REG_OSC_CONTROL);
	value &= 0b11100000;
	value |= (1 << 4); // Switches to RC oscillator when using backup power
	value |= (1 << 3); // Switches to RC oscillator when the XT one fails
	writeByteToRegister(REG_OSC_CONTROL, value);
}

void RV1805C3_reduceLeakage() {
	uint8_t value;

	value = readRegister(REG_CONTROL2);
	value &= 0b11011111; // Set X to 0
	writeByteToRegister(REG_CONTROL2, value);

	// Disable I2C interface when using backup power source
	writeByteToRegister(REG_CONFIG_KEY, CONFKEY_REGISTERS);
	writeByteToRegister(REG_IO_BATMODE, 0x00);

	// Disable WDI, !RST, and CLK/!INT when powered by backup source or in sleep mode
	writeByteToRegister(REG_CONFIG_KEY, CONFKEY_REGISTERS);
	writeByteToRegister(REG_OUTPUT_CONTROL, 0x30);
}

//-----works... I think? - yes it does

char* RV1805C3_getCurrentDateTime() {
	// Updates RTC date time value to array
	readBytesFromRegisters(REG_TIME_HUNDREDTHS, _dateTime, DATETIME_COMPONENTS);

	// Returns ISO 8601 date time string
	static char iso8601[23];
	sprintf(iso8601, "20%02d-%02d-%02dT%02d:%02d:%02d", 
			convertToDecimal(_dateTime[DATETIME_YEAR]),
			convertToDecimal(_dateTime[DATETIME_MONTH]),
			convertToDecimal(_dateTime[DATETIME_DAY_OF_MONTH]),
			convertToDecimal(_dateTime[DATETIME_HOUR]),
			convertToDecimal(_dateTime[DATETIME_MINUTE]),
			convertToDecimal(_dateTime[DATETIME_SECOND]));
	return iso8601;
}

//----


void RV1805C3_setDateTimeFromISO8601(const char *iso8601) {
	// Assumes the input is in the format of "2018-01-01T08:00:00" (hundredths and time zone, if applicable, will be neglected)
	char components[3] = { '0', '0', '\0' };
	uint8_t buffer[6];


	for (uint8_t i = 2, j = 0; i < 20; i += 3, j++) {
		components[0] = iso8601[i];
		components[1] = iso8601[i + 1];
		buffer[j] = atoi(components);		//atoi converts string to an integer, 
	}

	// Since ISO 8601 date string does not indicate day of week, it is set to 0 (Sunday) and is no longer correct
	RV1805C3_setDateTime(2000 + buffer[0], buffer[1], buffer[2], SUN, buffer[3], buffer[4], buffer[5], 0);
}

// void RV1805C3_setDateTimeFromHTTPHeader(string str) {
// 	return setDateTimeFromHTTPHeader(str.c_str());
// }

void RV1805C3_setDateTimeFromHTTPHeader(const char* str) {
	char components[3] = { '0', '0', '\0' };
	
	// Checks whether the string begins with "Date: " prefix
	uint8_t RV1805counter = 0;
	if (str[0] == 'D') {
		RV1805counter = 6;
	}
	
	// Day of week
	uint8_t dayOfWeek = 0;
	if (str[RV1805counter] == 'T') {
		// Tue or Thu
		if (str[RV1805counter + 1] == 'u') {
			// Tue
			dayOfWeek = 2;
		} else {
			dayOfWeek = 4;
		}
	} else if (str[RV1805counter] == 'S') {
		// Sat or Sun
		if (str[RV1805counter + 1] == 'a') {
			dayOfWeek = 6;
		} else {
			dayOfWeek = 0;
		}
	} else if (str[RV1805counter] == 'M') {
		// Mon
		dayOfWeek = 1;
	} else if (str[RV1805counter] == 'W') {
		// Wed
		dayOfWeek = 3;
	} else {
		// Fri
		dayOfWeek = 5;
	}

	// Day of month
	RV1805counter += 5;
	components[0] = str[RV1805counter];
	components[1] = str[RV1805counter + 1];
	uint8_t dayOfMonth = atoi(components);

	// Month
	RV1805counter += 3;
	uint8_t month = 0;
	if (str[RV1805counter] == 'J') {
		// Jan, Jun, or Jul
		if (str[RV1805counter + 1] == 'a') {
			// Jan
			month = 1;
		} else if (str[RV1805counter + 2] == 'n') {
			// Jun
			month = 6;
		} else {
			// Jul
			month = 7;
		}
	} else if (str[RV1805counter] == 'F') {
		// Feb
		month = 2;
	} else if (str[RV1805counter] == 'M') {
		// Mar or May
		if (str[RV1805counter + 2] == 'r') {
			// Mar
			month = 3;
		} else {
			// May
			month = 5;
		}
	} else if (str[RV1805counter] == 'A') {
		// Apr or Aug
		if (str[RV1805counter + 1] == 'p') {
			// Apr
			month = 4;
		} else {
			// Aug
			month = 8;
		}
	} else if (str[RV1805counter] == 'S') {
		// Sep
		month = 9;
	} else if (str[RV1805counter] == 'O') {
		// Oct
		month = 10;
	} else if (str[RV1805counter] == 'N') {
		// Nov
		month = 11;
	} else {
		month = 12;
	}

	// Year
	RV1805counter += 6;
	components[0] = str[RV1805counter];
	components[1] = str[RV1805counter + 1];
	uint16_t year = 2000 + atoi(components);

	// Time of day
	RV1805counter += 3;
	uint8_t buffer[3];
	for (uint8_t i = RV1805counter, j = 0; j < 3; i += 3, j++) {
		components[0] = str[i];
		components[1] = str[i + 1];
		buffer[j] = atoi(components);
	}
	
	RV1805C3_setDateTime(year, month, dayOfMonth, (DayOfWeek_t)dayOfWeek, buffer[0], buffer[1], buffer[2], 0);
}


bool RV1805C3_setDateTime(uint16_t year, uint8_t month, uint8_t dayOfMonth, DayOfWeek_t dayOfWeek, uint8_t hour, uint8_t minute, uint8_t second, uint8_t hundredth) {
	// Year 2000 AD is the earliest allowed year in this implementation
	if (year < 2000) { return false; }
	// Century overflow is not considered yet (i.e., only supports year 2000 to 2099)
	_dateTime[DATETIME_YEAR] = convertToBCD(year - 2000);

	if (month < 1 || month > 12) { return false; }
	_dateTime[DATETIME_MONTH] = convertToBCD(month);

	if (dayOfMonth < 1 || dayOfMonth > 31) { return false; }
	_dateTime[DATETIME_DAY_OF_MONTH] = convertToBCD(dayOfMonth);

	if (dayOfWeek > 6) { return false; }
	_dateTime[DATETIME_DAY_OF_WEEK] = convertToBCD(dayOfWeek);

	// Uses 24-hour notation by default
	if (hour > 23) { return false; }
	_dateTime[DATETIME_HOUR] = convertToBCD(hour);

	if (minute > 59) { return false; }
	_dateTime[DATETIME_MINUTE] = convertToBCD(minute);

	if (second > 59) { return false; }
	_dateTime[DATETIME_SECOND] = convertToBCD(second);

	if (hundredth > 99) { return false; }
	_dateTime[DATETIME_HUNDREDTH] = convertToBCD(hundredth);

	return true;
}

void RV1805C3_setDateTimeComponent(DateTimeComponent_t component, uint8_t value) {
	// Updates RTC date time value to array
	readBytesFromRegisters(REG_TIME_HUNDREDTHS, _dateTime, DATETIME_COMPONENTS);

	_dateTime[component] = convertToBCD(value);
}

void RV1805C3_synchronize() {
	writeBytesToRegisters(REG_TIME_HUNDREDTHS, _dateTime, DATETIME_COMPONENTS);
	return;
}

void RV1805C3_setAlarmMode(AlarmMode_t mode) {
	uint8_t value = readRegister(REG_COUNTDOWN_CONTROL);
	value &= 0b11100011;

	if (mode == ALARM_ONCE_PER_TENTH) {
		writeByteToRegister(REG_ALARM_HUNDREDTHS, 0xF0);
		value |= (7 << 2);
	} else if (mode == ALARM_ONCE_PER_HUNDREDTH) {
		writeByteToRegister(REG_ALARM_HUNDREDTHS, 0xFF);
		value |= (7 << 2);
	} else {
		value |= (mode << 2);
	}

	writeByteToRegister(REG_COUNTDOWN_CONTROL, value);
}


void RV1805C3_setAlarmFromISO8601(const char *iso8601) {
	RV1805C3_setDateTimeFromISO8601(iso8601);
	return writeBytesToRegisters(REG_ALARM_HUNDREDTHS, _dateTime, DATETIME_COMPONENTS);
}


void RV1805C3_setAlarmFromHTTPHeader(const char *str) {
	RV1805C3_setDateTimeFromHTTPHeader(str);
	writeBytesToRegisters(REG_ALARM_HUNDREDTHS, _dateTime, DATETIME_COMPONENTS);
	return;
}

int RV1805C3_setAlarm(uint16_t year, uint8_t month, uint8_t dayOfMonth, DayOfWeek_t dayOfWeek, uint8_t hour, uint8_t minute, uint8_t second, uint8_t hundredth) {
	if (!RV1805C3_setDateTime(year, month, dayOfMonth, dayOfWeek, hour, minute, second, hundredth)) {
		return 1;
	}

	writeBytesToRegisters(REG_ALARM_HUNDREDTHS, _dateTime, DATETIME_COMPONENTS);
	return 0;
}

void RV1805C3_setCountdownTimer(uint8_t period, CountdownUnit_t unit, bool repeat, bool interruptAsPulse) {
	if (period == 0) { return; }

	// Sets timer value
	writeByteToRegister(REG_COUNTDOWN_TIMER, period - 1);
	writeByteToRegister(REG_TIMER_INIT_VALUE, period - 1);

	// Enables timer
	uint8_t value = readRegister(REG_COUNTDOWN_CONTROL);
	value &= 0b00011100; // Clears countdown timer bits while preserving ARPT
	value |= unit; // Sets clock frequency
	value |= (!interruptAsPulse << 6);
	value |= (repeat << 5);
	value |= (1 << 7); // Timer enable
	writeByteToRegister(REG_COUNTDOWN_CONTROL, value);
}

void RV1805C3_enableInterrupt(InterruptType_t type) {
	uint8_t value = readRegister(REG_INTERRUPT_MASK);
	value |= (1 << type);
	writeByteToRegister(REG_INTERRUPT_MASK, value);
}

void RV1805C3_disableInterrupt(InterruptType_t type) {
	uint8_t value = readRegister(REG_INTERRUPT_MASK);
	value &= ~(1 << type);
	writeByteToRegister(REG_INTERRUPT_MASK, value);
}

uint8_t RV1805C3_clearInterrupts() {
	uint8_t value = readRegister(REG_STATUS);
	uint8_t flags = (value & 0b00111110);
	value &= 0b11000001;
	writeByteToRegister(REG_STATUS, value);
	return (flags >> 1);
}

void RV1805C3_sleep(SleepWaitPeriod_t waitPeriod, bool disableInterface) {
	uint8_t value;

	if (disableInterface) {
		value = readRegister(REG_OSC_CONTROL);
		value &= 0b11111011;
		value |= (1 << 2); // Disables I2C interface during sleep, use with caution!
		writeByteToRegister(REG_CONFIG_KEY, CONFKEY_OSC_CONTROL);
		writeByteToRegister(REG_OSC_CONTROL, value);
	}

	value = readRegister(REG_SLEEP_CONTROL);
	value |= (1 << 7); // Sleep Request
	value |= waitPeriod; // Sleep Wait
	writeByteToRegister(REG_SLEEP_CONTROL, value);
}

void RV1805C3_setPowerSwitchFunction(PowerSwitchFunction_t function) {
	uint8_t value = readRegister(REG_CONTROL2);
	value &= 0b11100011;
	value |= (function << 2);
	writeByteToRegister(REG_CONTROL2, value);
}

void RV1805C3_lockPowerSwitch() {
	uint8_t value = readRegister(REG_OSC_STATUS);
	value |= (1 << 5);
	writeByteToRegister(REG_OSC_STATUS, value);
}

void RV1805C3_unlockPowerSwitch() {
	uint8_t value = readRegister(REG_OSC_STATUS);
	value &= ~(1 << 5);
	writeByteToRegister(REG_OSC_STATUS, value);
}

void RV1805C3_setStaticPowerSwitchOutput(uint8_t level) {
	uint8_t value = readRegister(REG_CONTROL1);
	value &= ~(1 << 5);
	value |= ((level == 0 ? 0 : 1) << 5);
	writeByteToRegister(REG_CONTROL1, value);
}

static uint8_t convertToDecimal(uint8_t bcd) {
	return (bcd / 16 * 10) + (bcd % 16);
}


static uint8_t convertToBCD(uint8_t decimal) {
	return (decimal / 10 * 16) + (decimal % 10);
}

//----------------------------------------------I/O-----------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////////////////////


static uint8_t readRegister(uint8_t reg){
	return I2C_0_read1ByteRegister(RV1805C3_ADDRESS, reg);
}

static void readBytesFromRegisters(uint8_t startAddress, uint8_t *destination, uint8_t length) {
			for(int n=0; n<length; n++){
				destination[n] = readRegister(startAddress + n);
			}	
}

static void writeByteToRegister(uint8_t reg, uint16_t data){
	I2C_0_write1ByteRegister(RV1805C3_ADDRESS,	reg,	data);
}

static void writeBytesToRegisters(uint8_t startAddress, uint8_t *values, uint8_t length) {
			for(int n=0; n<length; n++){
				writeByteToRegister(startAddress + n, values[n]);
			}
}

