#ifndef VG_APP_H
#define VG_APP_H

void vg_app_init(void);

/* QA / demo boot: scenario 1-6 (0=keep default); page: home|alarm|diagnosis|logs|NULL */
void vg_app_boot(int scenario, const char * page);

#endif
