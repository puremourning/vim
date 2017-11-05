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
int pum_get_height(pum_T* pum);
/* vim: set ft=c : */
