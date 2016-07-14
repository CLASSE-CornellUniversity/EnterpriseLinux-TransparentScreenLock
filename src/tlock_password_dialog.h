/*
 ============================================================================
 Name        : tlock_password_dialog.h
 Author      : akaan
 Version     : 
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Apr 8, 2014
 ============================================================================
 */

#ifndef _TLOCK_PASSWORD_DIALOG_H_
#define _TLOCK_PASSWORD_DIALOG_H_

#include "tlock.h"

struct aXInfo;
struct aDialog;
struct aButton;

struct aDialog* tlock_create_dialog(struct aXInfo* xi,
        int x,
        int y,
        int width,
        int height,
        int line_width);

void flag_redraw(struct aDialog* dialog);
Bool is_area_pressed(struct aDialog* dialog, const char* name, int x, int y);
void tlock_draw_dialog(struct aXInfo* xi, struct aDialog* dialog, char* nameref, char* pwdref);
void tlock_show_dialog(struct aXInfo* xi, struct aDialog* dialog);
void tlock_hide_dialog(struct aXInfo* xi, struct aDialog* dialog);
void tlock_free_dialog(struct aXInfo* xi, struct aDialog* dialog);

#endif /* _TLOCK_PASSWORD_DIALOG_H_ */
