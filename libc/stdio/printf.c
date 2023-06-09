#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool print(const char* data, size_t length) {
  const unsigned char* bytes = (const unsigned char*)data;
  for (size_t i = 0; i < length; i++)
    if (putchar(bytes[i]) == EOF)
      return false;
  return true;
}

int printf(const char* restrict format, ...) {
  va_list parameters;
  va_start(parameters, format);
	int result = _printf(format, parameters);
	va_end(parameters);
	return result;
}

int _printf(const char* restrict format, va_list parameters) {

  int written = 0;

  while (*format != '\0') {
    size_t maxrem = INT_MAX - written;

    if (format[0] != '%' || format[1] == '%') {
      if (format[0] == '%')
        format++;
      size_t amount = 1;
      while (format[amount] && format[amount] != '%')
        amount++;
      if (maxrem < amount) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      if (!print(format, amount))
        return -1;
      format += amount;
      written += amount;
      continue;
    }

    const char* format_begun_at = format++;

    if (*format == 'c') {
      format++;
      char c = (char)va_arg(parameters, int /* char promotes to int */);
      if (!maxrem) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      if (!print(&c, sizeof(c)))
        return -1;
      written++;
    } else if (*format == 's') {
      format++;
      const char* str = va_arg(parameters, const char*);
      size_t len = strlen(str);
      if (maxrem < len) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      if (!print(str, len))
        return -1;
      written += len;
    } else if (*format == 'X' || *format == 'x') {
      uint8_t base = 16;
      bool isCapital = *format == 'X';
      format++;
      uint32_t value = va_arg(parameters, const uint32_t);

      const bool is_neg = value < 0;
      value *= is_neg ? -1 : 1;
      uint32_t len = value == 0 ? 1 : 0;
      uint32_t tmp = value;

      while (tmp != 0) {
        tmp /= base;
        ++len;
      }

      char str[len];
      for (uint32_t i = 0, tmp = value; i < len; ++i) {
        uint8_t digit = tmp % base;
        str[len - i - 1] = (char)(digit + (digit < 10 ? '0' : isCapital ? '7'
                                                                        : 'W'));
        tmp /= base;
      }

      if (is_neg) {
        written++;

        if (!print("-", 1))
          return -1;
      }

      if (len != 0) {
        written += 2;
        if (!print("0x", 2))
          return -1;
      }

      if (!print(str, len))
        return -1;

      written += len;
    } else if (*format == 'd' || *format == 'u') {
      uint8_t base = 10;
      format++;
      int32_t value = va_arg(parameters, const int32_t);

      const bool is_neg = value < 0;
      value *= is_neg ? -1 : 1;
      uint32_t len = value == 0 ? 1 : 0;
      int32_t tmp = value;

      while (tmp != 0) {
        tmp /= base;
        ++len;
      }

      char str[len];
      for (uint32_t i = 0, tmp = value; i < len; ++i) {
        uint8_t digit = tmp % base;
        str[len - i - 1] = (char)(digit + (digit < 10 ? '0' : '7'));
        tmp /= base;
      }

      if (is_neg) {
        written++;

        if (!print("-", 1))
          return -1;
      }

      if (!print(str, len))
        return -1;

      written += len;
    } else {
      format = format_begun_at;
      size_t len = strlen(format);
      if (maxrem < len) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      if (!print(format, len))
        return -1;
      written += len;
      format += len;
    }
  }

  return written;
}

int fprintf(struct FILE* stream, const char* format, ...) {
	va_list parameters;
  va_start(parameters, format);
	int result = _printf(format, parameters);
	va_end(parameters);
	return result;
}

int fflush(struct FILE* stream) {
  return 1;
}
