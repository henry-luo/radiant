#pragma once

#include <stddef.h>
#include <stdbool.h>

struct DomElement;

typedef enum RadiantInputValueKind {
    RADIANT_INPUT_VALUE_TEXT = 0,
    RADIANT_INPUT_VALUE_NUMBER,
    RADIANT_INPUT_VALUE_RANGE,
    RADIANT_INPUT_VALUE_DATE,
    RADIANT_INPUT_VALUE_MONTH,
    RADIANT_INPUT_VALUE_WEEK,
    RADIANT_INPUT_VALUE_TIME,
    RADIANT_INPUT_VALUE_DATETIME_LOCAL,
    RADIANT_INPUT_VALUE_COLOR,
    RADIANT_INPUT_VALUE_FILE,
    RADIANT_INPUT_VALUE_UNSUPPORTED
} RadiantInputValueKind;

typedef struct RadiantInputValidity {
    bool has_value;
    bool value_valid;
    bool range_underflow;
    bool range_overflow;
    bool step_mismatch;
} RadiantInputValidity;

#ifdef __cplusplus
extern "C" {
#endif

RadiantInputValueKind radiant_input_value_kind(const char* type);
const char* radiant_input_type_normalize(const char* type, char* output,
                                         size_t output_size);
bool radiant_input_value_sanitize(const char* type, const char* value,
                                  char* output, size_t output_size);
bool radiant_input_value_as_number(const char* type, const char* value,
                                   double* output);
bool radiant_input_value_from_number(const char* type, double value,
                                     char* output, size_t output_size);
bool radiant_input_value_as_date_supported(const char* type);
void radiant_input_value_validate(const char* type, const char* value,
                                  const char* min_value, const char* max_value,
                                  const char* step_value,
                                  RadiantInputValidity* output);
bool radiant_input_value_step(const char* type, const char* value,
                              const char* min_value, const char* max_value,
                              const char* step_value, int count,
                              char* output, size_t output_size);

bool radiant_input_ensure_live_value(struct DomElement* element);
const char* radiant_input_live_value(struct DomElement* element);
const char* radiant_input_peek_live_value(struct DomElement* element);
bool radiant_input_set_live_value(struct DomElement* element, const char* value);
void radiant_input_reset_live_value(struct DomElement* element);
void radiant_input_type_changed(struct DomElement* element);

#ifdef __cplusplus
}
#endif
