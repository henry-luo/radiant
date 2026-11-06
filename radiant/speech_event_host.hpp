#pragma once

#include <stdint.h>

struct DomElement;
// Fields and strings are borrowed only for the synchronous publication call.
struct SpeechEventFields {
    const char* kind;
    const char* name;
    int64_t char_index, char_length;
    double time;
    double delay;
    bool cancelled;
    const char* reason;
};
struct RadiantSpeechEventHostOps {
    bool (*publish)(DomElement* target, uint64_t token, const char* type, const SpeechEventFields* fields);
};

bool radiant_speech_event_host_register(const RadiantSpeechEventHostOps* operations);
bool radiant_speech_event_publish(DomElement* target, uint64_t token, const char* type,
                                  const SpeechEventFields* fields);
