# A C library for the RV1805C3 real time clock. Based on MacroYau's c++ library.

Supports Micro Crystal [RV-1805-C3](http://www.microcrystal.com/images/_Product-Documentation/02_Oscillator_&_RTC_Modules/01_Datasheet/RV-1805-C3.pdf) extreme low power RTC module.

## Usage

- `SetDateTime`: Configures date and time to the RTC module via the serial console, and then
  prints the current date and time in ISO 8601 format every second.
- `AlarmInterrupt`: Sets an alarm based on calendar date and time, and triggers an interrupt.
- `CountdownTimer`: Sets a repeating countdown timer that triggers an interrupt.

## Compatibility

  working with asf4's I2C library. Can easily be modified for your needs by editing the I/O functions

## License

MIT
