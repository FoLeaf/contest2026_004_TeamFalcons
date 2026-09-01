#ifndef __APP_VELAGUARD_VG_AGENT_SEED_H
#define __APP_VELAGUARD_VG_AGENT_SEED_H

#ifdef CONFIG_EXAMPLES_AI_AGENT_VELA
void vg_agent_seed_content(void);
#else
static inline void vg_agent_seed_content(void) {}
#endif

#endif
