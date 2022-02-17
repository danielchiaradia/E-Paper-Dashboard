/*
 * Logger.h
 *
 *  Created on: 27.01.2019
 *      Author: Daniel Chiaradia
 */

#ifndef LOGGER_H_
#define LOGGER_H_
#include <WString.h>
#include <Arduino.h>
#include <stdarg.h>

class Log {
public:
	Log(bool enabled) {
		// if (enabled)
		// {
	
		// }
	}

	void setEnable(bool value) {
		enabled = value;
	}

	void log(char* msg, ...) {
		if (enabled) {
			bufferString.remove(0);
			va_list args;
			va_start(args, msg);
			print(msg, args, bufferString);

			Serial.println(bufferString);
		}
	}

private:
	String bufferString;
	bool enabled = true;

	void print(const char *format, va_list args, String& dst) {
		for (; *format != 0; ++format) {
			if (*format == '%') {
				++format;
				if (*format == '\0')
					break;
				else if (*format == '%') {
					dst.concat(*format);
					continue;
				} else if (*format == 's') {
					register char *s = (char *) va_arg(args, int);
					dst.concat(s);
					continue;
				} else if (*format == 'd' || *format == 'i' || *format == 'c') {
					dst.concat(va_arg(args, int));
					continue;
				} else if (*format == 'f') {
					dst.concat(va_arg(args, double));
					continue;
				} else if (*format == 'l') {
					dst.concat(va_arg(args, long));
					continue;
				} else if (*format == 't') {
					if (va_arg(
							args, int) == 1) {
						dst.concat('T');
					} else {
						dst.concat('F');
					}
					continue;
				} else if (*format == 'T') {
					if (va_arg(
							args, int) == 1) {
						dst.concat("true");
					} else {
						dst.concat("false");
					}
					continue;
				}
			}
			dst.concat(*format);
		}
	}
};

Log Logger = Log(true);

#endif /* LOGGER_H_ */
