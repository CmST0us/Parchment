/**
 * @file sim_battery.c
 * @brief Battery stub for simulator. Always reports 100%.
 */
void battery_init(void) {}
int battery_get_percent(void) { return 100; }
