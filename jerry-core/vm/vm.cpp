/* Copyright 2015 University of Szeged.
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

#include "ecma-helpers.h"
#include "vm.h"

const cbc_compiled_code_t *__program = NULL;

/**
 * Initialize interpreter.
 */
void
vm_init (const cbc_compiled_code_t *program_p, /**< pointer to byte-code data */
         bool dump_mem_stats) /** dump per-instruction memory usage change statistics */
{
#ifdef MEM_STATS
  interp_mem_stats_enabled = dump_mem_stats;
#else /* MEM_STATS */
  JERRY_ASSERT (!dump_mem_stats);
#endif /* !MEM_STATS */

  JERRY_ASSERT (__program == NULL);

  __program = program_p;
} /* vm_init */

/**
 * Cleanup interpreter
 */
void
vm_finalize (void)
{
  if (__program)
  {
    mem_heap_free_block (__program);
  }
} /* vm_finalize */

/**
 * Run global code
 */
jerry_completion_code_t
vm_run_global (void)
{
  jerry_completion_code_t ret_code;
  // FIXME: Implement this



  return ret_code;
} /* vm_run_global */

/**
 * Run specified eval-mode bytecode
 *
 * @return completion value
 */
ecma_completion_value_t
vm_run_eval (const cbc_compiled_code_t *bytecode_data_p, /**< byte-code data header */
             bool is_direct) /**< is eval called in direct mode? */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  return ret_value;
} /* vm_run_eval */

ecma_completion_value_t vm_loop (vm_frame_ctx_t *)
{
// FIXME: Implement this
}

ecma_completion_value_t vm_run_from_pos (const cbc_compiled_code_t *,
                                         vm_instr_counter_t,
                                         ecma_value_t,
                                         ecma_object_t *,
                                         bool,
                                         bool)
{
// FIXME: Implement this
}

opcode_scope_code_flags_t vm_get_scope_flags (const cbc_compiled_code_t *)
{
// FIXME: Implement this
}


bool vm_is_strict_mode (void)
{
// FIXME: Implement this
  return __program->code_flags & CBC_CODE_FLAGS_STRICT_MODE;
}

bool vm_is_direct_eval_form_call (void)
{
// FIXME: Implement this
}

ecma_value_t vm_get_this_binding (void)
{
// FIXME: Implement this
}

ecma_object_t *vm_get_lex_env (void)
{
// FIXME: Implement this
}
