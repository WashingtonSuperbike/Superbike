/**
*/

#pragma once

// Kick the hardware watchdog. Call from the highest-priority task (preChargeTask) each loop.
extern void wdt_kick();
