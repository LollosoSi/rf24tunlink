#pragma once

#include <fcntl.h>
#include <cstdint>
#include <unistd.h>

enum HARQEventType { FRAME_COMPLETED, FRAME_COMPLETED_TIMEOUT } type;
struct HARQInternalEvent {
    HARQEventType type;
    uint8_t buffer_id;     // ID del frame a cui si riferisce
};

int internal_event_pipe[2]; // [0] Read (Main Thread), [1] Write (RX Thread)

void initialize_fd_internalevents(){
    pipe2(internal_event_pipe, O_CLOEXEC | O_NONBLOCK);
}

int get_harq_internal_fd_pipe(){
    return internal_event_pipe[0];
}

void send_internal_event(HARQEventType type, uint8_t id){
    HARQInternalEvent event = {type, id};
    write(internal_event_pipe[1], &event, sizeof(event));
}