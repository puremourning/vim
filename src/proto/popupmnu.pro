/* popupmnu.c */
void pum_display(pum_T* pum,
                 int invert_position,
                 pumitem_T *array,
                 int size,
                 int selected);
void pum_redraw(pum_T* pum);
void pum_undisplay(pum_T* pum);
void pum_clear(pum_T* pum);
int pum_visible(pum_T* pum);
void pum_may_redraw(pum_T* pum);
int pum_get_height(pum_T* pum);
int split_message(char_u *mesg, pumitem_T **array);
void ui_remove_balloon(pum_T* pum);
void ui_post_balloon(pum_T* pum, char_u *mesg, list_T *list);
void ui_may_remove_balloon(pum_T* pum);
void pum_show_popupmenu(pum_T* pum, vimmenu_T *menu);
void pum_make_popup(pum_T* pum, char_u *path_name, int use_mouse_pos);
/* vim: set ft=c : */
