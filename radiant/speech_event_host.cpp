#include "speech_event_host.hpp"
#include "lib/provider.hpp"

static const RadiantSpeechEventHostOps* speech_event_host_ops = nullptr;

bool radiant_speech_event_host_register(const RadiantSpeechEventHostOps* operations) {
    return lambda_provider_register(&speech_event_host_ops, operations, &RadiantSpeechEventHostOps::publish);
}

bool radiant_speech_event_publish(DomElement* target, uint64_t token, const char* type,
                                  const SpeechEventFields* fields) {
    const RadiantSpeechEventHostOps* operations = lambda_provider_load(&speech_event_host_ops);
    return operations && operations->publish(target, token, type, fields);
}
