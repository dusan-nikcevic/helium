#pragma once
#include "ardour/session.h"
#include <pthread.h>
#include <memory>

// Capture native control operations on the caller thread for one atomic process event.
class EngineSession : public ARDOUR::Session {
    pthread_t owner = pthread_self();
    bool capturing = false;
    std::unique_ptr<ARDOUR::SessionEvent> captured;
public:
    using ARDOUR::Session::Session;
    std::unique_ptr<ARDOUR::SessionEvent> controlEvent(std::shared_ptr<ARDOUR::AutomationControl> control, double value) {
        capturing = true;
        try { set_control(control, value, PBD::Controllable::NoGroup); }
        catch (...) { capturing = false; captured.reset(); throw; }
        capturing = false;
        return std::move(captured);
    }
    void queue_event(ARDOUR::SessionEvent *event) override {
        if (pthread_equal(owner, pthread_self()) && capturing) captured.reset(event);
        else ARDOUR::Session::queue_event(event);
    }
};
