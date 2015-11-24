/* Copyright 2014-2015 Samsung Electronics Co., Ltd.
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

#include "jerry-core/jerry.h"

#include <stdlib.h>
#include <stdio.h>

#include "native_mbedk64f.h"


#define JERRY_STANDALONE_EXIT_CODE_OK   (0)
#define JERRY_STANDALONE_EXIT_CODE_FAIL (1)

#ifndef MAX
#define MAX(A,B) ((A)>(B)?(A):(B))
#endif


static const char* fn_sys_loop_name = "sysloop";

//-----------------------------------------------------------------------------

#define API_DATA_IS_OBJECT(val_p) ((val_p)->type == JERRY_API_DATA_TYPE_OBJECT)

#define API_DATA_IS_FUNCTION(val_p) (API_DATA_IS_OBJECT(val_p) && jerry_api_is_function((val_p)->v_object))

#define JS_VALUE_TO_NUMBER(val_p) \
    ((val_p)->type == JERRY_API_DATA_TYPE_FLOAT32 ? \
       static_cast<double>((val_p)->v_float32) : \
     (val_p)->type == JERRY_API_DATA_TYPE_FLOAT64 ? \
       static_cast<double>((val_p)->v_float64) : \
     static_cast<double>((val_p)->v_uint32))

#define __UNSED__ __attribute__((unused))

#define DELCARE_HANDLER(NAME) \
static bool \
NAME ## _handler (const jerry_api_object_t * function_obj_p __UNSED__, \
                  const jerry_api_value_t *  this_p __UNSED__, \
                  jerry_api_value_t *        ret_val_p __UNSED__, \
                  const jerry_api_value_t    args_p[], \
                  const jerry_api_length_t   args_cnt)

#define REGISTER_HANDLER(NAME) \
  register_native_function ( # NAME, NAME ## _handler)

//-----------------------------------------------------------------------------

DELCARE_HANDLER(assert) {
  if (args_cnt == 1
      && args_p[0].type == JERRY_API_DATA_TYPE_BOOLEAN
      && args_p[0].v_bool == true)
  {
    printf (">> Jerry assert true\r\n");
    return true;
  }
  printf ("ERROR: Script assertion failed\n");
  exit (JERRY_STANDALONE_EXIT_CODE_FAIL);
  return false;
}


DELCARE_HANDLER(print) {
  jerry_api_length_t cc;

  if (args_cnt)
  {
    printf(">> print(%d) :", (int)args_cnt);
    for (cc=0; cc<args_cnt; cc++)
    {
      if (args_p[cc].type == JERRY_API_DATA_TYPE_STRING && args_p[cc].v_string)
      {
        static char buffer[128];
        int length, maxlength;
        length = -jerry_api_string_to_char_buffer (args_p[0].v_string, NULL, 0);
        maxlength  = MAX(length, 126);
        jerry_api_string_to_char_buffer (args_p[cc].v_string,
                                         (jerry_api_char_t *) buffer,
                                         maxlength);
        *(buffer + length) = 0;
        printf("[%s] (%d) ", buffer, length);
      }
      else
      {
        printf ("(%d) ", args_p[cc].type);
      }
    }
    printf ("\r\n");
  }
  return true;
}


DELCARE_HANDLER(led) {
  if (args_cnt < 2)
  {
    return false;
  }

  int port, value;
  port = (int)JS_VALUE_TO_NUMBER (&args_p[0]);
  value = (int)JS_VALUE_TO_NUMBER (&args_p[1]);

  ret_val_p->type = JERRY_API_DATA_TYPE_BOOLEAN;
  if (port >=0 && port <= 3) {
    native_led(port, value);
    ret_val_p->v_bool = true;
  }
  else {
    ret_val_p->v_bool = false;
  }
  return true;
} /* gpio_dir_handler */


//-----------------------------------------------------------------------------

static bool
register_native_function (const char* name,
                          jerry_external_handler_t handler)
{
  jerry_api_object_t *global_obj_p;
  jerry_api_object_t *reg_func_p;
  jerry_api_value_t reg_value;
  bool bok;

  global_obj_p = jerry_api_get_global ();
  reg_func_p = jerry_api_create_external_function (handler);

  if (!(reg_func_p != NULL
                && jerry_api_is_function (reg_func_p)
                && jerry_api_is_constructor (reg_func_p)))
  {
    printf (">> create_external_function failed !!!\r\n");
    jerry_api_release_object (global_obj_p);
    return false;
  }

  jerry_api_acquire_object (reg_func_p);
  reg_value.type = JERRY_API_DATA_TYPE_OBJECT;
  reg_value.v_object = reg_func_p;

  bok = jerry_api_set_object_field_value (global_obj_p,
                                          (jerry_api_char_t *)name,
                                          &reg_value);

  jerry_api_release_value (&reg_value);
  jerry_api_release_object (reg_func_p);
  jerry_api_release_object (global_obj_p);

  if (!bok)
  {
    printf (">> register_native_function failed: [%s]\r\n", name);
  }

  return bok;
}


//-----------------------------------------------------------------------------

static void register_functions (void)
{
  REGISTER_HANDLER(assert);
  REGISTER_HANDLER(print);
  REGISTER_HANDLER(led);
}


//-----------------------------------------------------------------------------

int js_entry (const char *source_p, const size_t source_size)
{
  const jerry_api_char_t *jerry_src = (const jerry_api_char_t *) source_p;
  jerry_completion_code_t ret_code = JERRY_COMPLETION_CODE_OK;
  jerry_flag_t flags = JERRY_FLAG_EMPTY;

  printf ("\r\nJerryScript in mbed K64F\r\n");
  printf ("   build  %s\r\n", jerry_build_date);
  printf ("   hash   %s\r\n", jerry_commit_hash);
  printf ("   branch %s\r\n", jerry_branch_name);

  jerry_init (flags);
  printf(">> jerry_init ok\r\n");

  register_functions ();

  if (!jerry_parse (jerry_src, source_size))
  {
    printf (">> jerry_parse failed\r\n");
    ret_code = JERRY_COMPLETION_CODE_UNHANDLED_EXCEPTION;
  }
  else
  {
    printf(">> jerry_parse ok\r\n");
    if ((flags & JERRY_FLAG_PARSE_ONLY) == 0)
    {
      ret_code = jerry_run ();
      printf(">> jerry_run ok\r\n");
    }
  }

  return ret_code;
}

int js_eval (const char *source_p, const size_t source_size)
{
  jerry_completion_code_t status;
  jerry_api_value_t res;

  status = jerry_api_eval ((jerry_api_char_t *) source_p,
                           source_size,
                           false,
                           false,
                           &res);
  printf(">> jerry_api_eval status(%d)\r\n", status);

  jerry_api_release_value (&res);

  return status;
}

int js_loop (uint32_t ticknow)
{
  jerry_api_object_t *global_obj_p;
  jerry_api_value_t sysloop_func;
  bool is_ok;

  global_obj_p = jerry_api_get_global ();
  is_ok = jerry_api_get_object_field_value (global_obj_p,
                          (const jerry_api_char_t*)fn_sys_loop_name,
                          &sysloop_func);
  if (!is_ok)
  {
    printf (">> '%s' not defined!!!\r\n", fn_sys_loop_name);
    jerry_api_release_object (global_obj_p);
    return -1;
  }

  if (!API_DATA_IS_FUNCTION (&sysloop_func))
  {
    printf (">> '%s' is not a function!!!\r\n", fn_sys_loop_name);
    jerry_api_release_value (&sysloop_func);
    jerry_api_release_object (global_obj_p);
    return -2;
  }

  jerry_api_value_t* val_args;
  uint16_t val_argv;

  val_argv = 1;
  val_args = (jerry_api_value_t*)malloc (sizeof (jerry_api_value_t) * val_argv);
  val_args[0].type = JERRY_API_DATA_TYPE_UINT32;
  val_args[0].v_uint32 = ticknow;

  jerry_api_value_t res;
  is_ok = jerry_api_call_function (sysloop_func.v_object,
                                   global_obj_p,
                                   &res,
                                   val_args,
                                   val_argv);
  jerry_api_release_value (&res);
  free (val_args);

  jerry_api_release_value (&sysloop_func);
  jerry_api_release_object (global_obj_p);

  return 0;
}

void js_exit (void)
{
  jerry_cleanup ();
}

