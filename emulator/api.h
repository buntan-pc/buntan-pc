/*
 * Copylight (c) 2026 tas0dev
 */

#ifndef BUNTAN_EMU_CPU_H
#define BUNTAN_EMU_CPU_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void cpu_init(void);
void cpu_reset(void);
void cpu_step(void);
void cpu_destroy(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BUNTAN_EMU_CPU_H */
