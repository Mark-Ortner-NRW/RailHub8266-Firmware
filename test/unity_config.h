#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

// Unity configuration for ESP8266 testing

// Use putchar for output (works in both C and C++)
#ifndef UNITY_OUTPUT_CHAR
#define UNITY_OUTPUT_CHAR(a)    putchar(a)
#endif

// Exclude float support to save memory on ESP8266
#ifndef UNITY_EXCLUDE_FLOAT
#define UNITY_EXCLUDE_FLOAT
#endif

// Exclude double support to save memory on ESP8266
#ifndef UNITY_EXCLUDE_DOUBLE
#define UNITY_EXCLUDE_DOUBLE
#endif

#endif // UNITY_CONFIG_H
