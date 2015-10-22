#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__


#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC void gpio_dir_native (int, int);
EXTERNC void gpio_set_native (int, int);
EXTERNC int gpio_get_native (int);

EXTERNC int js_entry (const char *source_p, const size_t source_size);
EXTERNC int js_eval (const char *source_p, const size_t source_size);
EXTERNC int js_loop (uint32_t ticknow);
EXTERNC void js_exit (void);

#endif

