/* scriptfile.c */
void estack_init(void);
estack_T *estack_get_level(int level);
estack_T *estack_get_backtrace_level(int level);
int estack_max_backtrace_level();
estack_T *estack_push(etype_T type, char_u *name, long lnum);
estack_T *estack_push_ufunc(funccall_T *ufunc);
estack_T *estack_push_dfunc(ufunc_T *ufunc, long lnum);
int estack_top_is_ufunc(ufunc_T *ufunc, long lnum);
estack_T *estack_pop(void);
char_u *estack_sfile(estack_arg_T which);
void ex_runtime(exarg_T *eap);
int do_in_path(char_u *path, char_u *name, int flags, void (*callback)(char_u *fname, void *ck), void *cookie);
int do_in_runtimepath(char_u *name, int flags, void (*callback)(char_u *fname, void *ck), void *cookie);
int source_runtime(char_u *name, int flags);
int source_in_path(char_u *path, char_u *name, int flags, int *ret_sid);
void add_pack_start_dirs(void);
void load_start_packages(void);
void ex_packloadall(exarg_T *eap);
void ex_packadd(exarg_T *eap);
void remove_duplicates(garray_T *gap);
int ExpandRTDir(char_u *pat, int flags, int *num_file, char_u ***file, char *dirnames[]);
int ExpandPackAddDir(char_u *pat, int *num_file, char_u ***file);
void ex_source(exarg_T *eap);
void ex_options(exarg_T *eap);
linenr_T *source_breakpoint(void *cookie);
int *source_dbg_tick(void *cookie);
int source_level(void *cookie);
char_u *source_nextline(void *cookie);
int do_source(char_u *fname, int check_other, int is_vimrc, int *ret_sid);
void ex_scriptnames(exarg_T *eap);
void scriptnames_slash_adjust(void);
char_u *get_scriptname(scid_T id);
void free_scriptnames(void);
void free_autoload_scriptnames(void);
linenr_T get_sourced_lnum(char_u *(*fgetline)(int, void *, int, getline_opt_T), void *cookie);
char_u *getsourceline(int c, void *cookie, int indent, getline_opt_T options);
void ex_scriptencoding(exarg_T *eap);
void ex_scriptversion(exarg_T *eap);
void ex_finish(exarg_T *eap);
void do_finish(exarg_T *eap, int reanimate);
int source_finished(char_u *(*fgetline)(int, void *, int, getline_opt_T), void *cookie);
char_u *autoload_name(char_u *name);
int script_autoload(char_u *name, int reload);
/* vim: set ft=c : */
