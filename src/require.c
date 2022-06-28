#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/dump.h"
#include "mruby/string.h"
#include "mruby/proc.h"

#if defined(MRUBY_RELEASE_NO) && MRUBY_RELEASE_NO >= 10100
#include "mruby/opcode.h"
#include "mruby/error.h"
#else
#include "mruby/../../src/opcode.h"
#include "mruby/../../src/error.h"
#define MRUBY_RELEASE_NO 0
#endif

#include <stdlib.h>

#define E_LOAD_ERROR (mrb_class_get(mrb, "LoadError"))

#if MRUBY_RELEASE_NO < 10000
mrb_value mrb_yield_internal(mrb_state *mrb, mrb_value b, int argc, mrb_value *argv, mrb_value self, struct RClass *c);
#define mrb_yield_with_class mrb_yield_internal
#endif

#if MRUBY_RELEASE_NO < 10400
#define MRB_PROC_SET_TARGET_CLASS(p,tc) do { \
  (p)->target_class = (tc); \
} while (0)
#endif

// older than mruby-2.1.0
#ifndef mrb_proc_p
#define mrb_proc_p(o) (mrb_type(o) == MRB_TT_PROC)
#endif

#if MRUBY_RELEASE_NO >= 30000
#define ci_target_class_set(ci, tc) do { (ci)->u.target_class = (tc); } while (0)
#else
#define ci_target_class_set(ci, tc) do { (ci)->target_class = (tc); } while (0)
#endif

#if MRUBY_RELEASE_NO >= 30100
#define CI_CCI_DIRECT_P(ci) ((ci)->cci == 2 /* CINFO_DIRECT */)
#else
#define CI_CCI_DIRECT_P(ci) ((ci)->acc == -2 /* CI_ACC_DIRECT */)
#endif

#if MRUBY_RELEASE_NO >= 20000
#define replace_stop_with_return(mrb, irep) ((void)0)
#else
static void
replace_stop_with_return(mrb_state *mrb, mrb_irep *irep)
{
  if (irep->iseq[irep->ilen - 1] == MKOP_A(OP_STOP, 0)) {
    irep->iseq = mrb_realloc(mrb, irep->iseq, (irep->ilen + 1) * sizeof(mrb_code));
    irep->iseq[irep->ilen - 1] = MKOP_A(OP_LOADNIL, 0);
    irep->iseq[irep->ilen] = MKOP_AB(OP_RETURN, 0, OP_R_NORMAL);
    irep->ilen++;
  }
}
#endif

static void
eval_proc(mrb_state *mrb, mrb_value proc)
{
  replace_stop_with_return(mrb, mrb_proc_ptr(proc)->body.irep);
  MRB_PROC_SET_TARGET_CLASS(mrb_proc_ptr(proc), mrb->object_class);

#if MRUBY_RELEASE_NO >= 10300
  if (!CI_CCI_DIRECT_P(mrb->c->ci)) {
    ci_target_class_set(mrb->c->ci, mrb->object_class);
    mrb_yield_cont(mrb, proc, mrb_top_self(mrb), 0, NULL);
    return;
  }
#endif

  mrb_yield_with_class(mrb, proc, 0, NULL, mrb_top_self(mrb), mrb->object_class);
}

static mrb_value
mrb_require_load_file_common(mrb_state *mrb, mrb_value (*loader)(mrb_state *, FILE *, mrbc_context *))
{
  char *path_ptr = NULL;
  FILE *fp = NULL;
  mrb_value proc;
  mrb_value path;
  int ai = mrb_gc_arena_save(mrb);
  mrbc_context *mrbc;

  mrb_get_args(mrb, "S", &path);
  path_ptr = mrb_str_to_cstr(mrb, path);

  mrbc = mrbc_context_new(mrb);
  mrbc->no_exec = TRUE;
  mrbc->capture_errors = TRUE;
  mrbc_filename(mrb, mrbc, path_ptr);

  fp = fopen(path_ptr, "rb");
  if (fp == NULL) {
    mrbc_context_free(mrb, mrbc);
    mrb_raisef(mrb, E_LOAD_ERROR, "can't open file -- %S", path);
  }

  proc = loader(mrb, fp, mrbc);
  fclose(fp);
  mrbc_context_free(mrb, mrbc);

  if (mrb_proc_p(proc)) {
    eval_proc(mrb, proc);
  } else if (mrb->exc) {
    // fail to load
    mrb_exc_raise(mrb, mrb_obj_value(mrb->exc));
  } else {
    mrb_raisef(mrb, E_LOAD_ERROR, "can't load file -- %S", path);
    return mrb_nil_value();
  }

  mrb_gc_arena_restore(mrb, ai);

  return mrb_top_self(mrb); // used as self in loaded library with mrb_yield_cont()
}

static mrb_value
mrb_require_load_rb_file(mrb_state *mrb, mrb_value self)
{
  (void)self;

  return mrb_require_load_file_common(mrb, mrb_load_file_cxt);
}

static mrb_value
mrb_require_load_mrb_file(mrb_state *mrb, mrb_value self)
{
  (void)self;

  return mrb_require_load_file_common(mrb, mrb_load_irep_file_cxt);
}

void
mrb_mruby_require_gem_init(mrb_state *mrb)
{
  struct RClass *krn;
  krn = mrb->kernel_module;

  mrb_define_method(mrb, krn, "_load_rb_file",  mrb_require_load_rb_file,  MRB_ARGS_REQ(1));
  mrb_define_method(mrb, krn, "_load_mrb_file", mrb_require_load_mrb_file, MRB_ARGS_REQ(1));
}

void
mrb_mruby_require_gem_final(mrb_state *mrb)
{
}
