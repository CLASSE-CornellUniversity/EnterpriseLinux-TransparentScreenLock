/*
 ============================================================================
 Name        : tlock_frame.h
 Author      : akaan
 Version     : 
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Jan 25, 2014
 ============================================================================
 */

#ifndef _TLOCK_FRAME_H_
#define _TLOCK_FRAME_H_

/* ---------------------------------------------------------------- *
 about : functions to render a colored frame to the screen
 mostly for visual feedback purposes :)

 \* ---------------------------------------------------------------- */
struct aXInfo;
struct aFrame;

struct aFrame* tlock_create_frame(struct aXInfo* xi,
        int x,
        int y,
        int width,
        int height,
        int line_width);
void tlock_draw_frame(struct aXInfo* xi, struct aFrame* frame, const char* color_name);
void tlock_show_frame(struct aXInfo* xi, struct aFrame* frame);
void tlock_hide_frame(struct aXInfo* xi, struct aFrame* frame);
void tlock_free_frame(struct aXInfo* xi, struct aFrame* frame);

/* ---------------------------------------------------------------- *\
\* ---------------------------------------------------------------- */
#endif // _TLOCK_FRAME_H_

