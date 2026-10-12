#pragma once

// Glue between the ported DOS code and the platform layer.
//
// The original runs its timer and keyboard in interrupt handlers while the
// main code busy-waits. The port has no interrupts: every wait loop calls
// SYS_Poll(), which advances TimeCount by the real time elapsed and feeds key
// events to ID_IN, as the handlers would have.

#include "id/id_heads.h"

void SYS_Startup();
void SYS_Poll();
// SYS_Poll plus a short sleep, for busy-wait loops.
void SYS_Idle();
// Waits `tics` 70 Hz tics of real time while polling.
void SYS_WaitTics(int tics);
