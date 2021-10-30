#ifndef UNITY_SIMULATE_INPUT_EVENTS_
#define UNITY_SIMULATE_INPUT_EVENTS_

void SimulateInputEvents();
void SimulateMouseInput();

// Implemented by various platforms:
size_t GetActiveTouchCount ();

#endif
