/* Copyright 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __JERRY_ESP8266_H__
#define __JERRY_ESP8266_H__

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC extern
#endif

EXTERNC void gpio_dir_native (int, int);
EXTERNC void gpio_set_native (int, int);
EXTERNC int gpio_get_native (int);

EXTERNC int js_entry (const char *source_p, const size_t source_size);
EXTERNC int js_eval (const char *source_p, const size_t source_size);
EXTERNC int js_loop (uint32_t ticknow);
EXTERNC void js_exit (void);

#endif
