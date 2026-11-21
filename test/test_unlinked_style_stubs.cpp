#include "radiant/event.hpp"
#include <stdlib.h>

// These isolated targets have neither a style resolver nor a form-control host.
DisplayValue layout_resolved_display(void*) { abort(); }
void form_control_bind_document(DomElement*, FormControlProp*) { abort(); }
