/*
 *  This file is part of the mx text editor.
 *
 *  mx is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  mx is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EDITOR_GUARD
#define EDITOR_GUARD

#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <wchar.h>
#include <termios.h>
#include <errno.h>

#define TRUE  1
#define FALSE 0

#define DEBUG FALSE

#define LINE_BLOCK_SIZE 100
#define ROW_BLOCK_SIZE  100 /* has to be greater than 1 */

#define LINE_LEN  row_pointer->line_length
#define CURSOR    row_pointer->cursor
#define LINE_END  row_pointer->line_end
#define CUR_ROW   con->current_row
#define MAX_ROW   con->max_row
#define BUFFER    row_pointer->buffer
#define HPADDING  con->hpadding
#define VPADDING  con->vpadding

#define CURSOR_PREV   row_pointer_prev->cursor
#define LINE_END_PREV row_pointer_prev->line_end
#define BUFFER_PREV   row_pointer_prev->buffer

#define KEY_CTRL       -96
#define KEY_BACKSPACE  127
#define KEY_ALT         27
#define KEY_TAB          9
#define KEY_ENTER       10

#define ANSI_RESET_SCREEN        printf("\033[2J\033[1;1H")
#define ANSI_KILL_LINE           printf("\033[K")
#define ANSI_INVERT_COLOR        printf("\033[7m")
#define ANSI_REVERT_INVERT_COLOR printf("\033[27m")

/* Has to correlate to the tab width of the terminal
 * but this is not guaranteed if the user has set
 * the terminal tab with by himself. Should read
 * terminal tab width an set this value accordingly.
 */
#define TAB_STOP_WIDTH   8
#define TAB_PAD_CHAR    -9


enum draw_mode {
    WHOLE,
    REGION_DOWN,
    REGION_UP,
    LINE
};


typedef struct readline {
    int     cursor;
    int     line_end;
    wint_t *buffer;
    int     line_length;
} readline;

typedef struct container {
    readline *rows; 
    int       current_row;
    int       max_row;
    int       row_length; 
    int       hpadding;
    int       vpadding;
} container;

struct winsize w;

void      screen_redraw                     (container*, enum draw_mode);
void      die                               (const char*);
void      make_new_row                      (readline*);
void      editor_save_file                  (container*, char[]);
void      editor_load_file                  (container*, char[]);
void      infobar_print                     (container*, char[]);
void      infobar_error                     (container*, char[]);
void      infobar_erase                     (container*);
void      infobar_print_position            (container*);
readline* editor_newline                    (container*, readline*);
readline* editor_insert_tab                 (container*, readline*);
readline* editor_insert_char                (container*, readline*, wint_t);
readline* editor_delete_char                (container*, readline*, wint_t);
readline* editor_delete_forward_char        (container*, readline*, wint_t);
readline* editor_delete_line                (container*, readline*, wint_t);
readline* editor_forward_char               (container*, readline*, wint_t);
readline* editor_backward_char              (container*, readline*, wint_t);
readline* editor_forward_word               (container*, readline*, wint_t);
readline* editor_backward_word              (container*, readline*, wint_t);
readline* editor_move_beginning_of_line     (container*, readline*, wint_t);
readline* editor_move_end_of_line           (container*, readline*, wint_t);
readline* editor_move_next_line             (container*, readline*, wint_t);
readline* editor_move_previous_line         (container*, readline*, wint_t);
readline* editor_page_down                  (container*, readline*, wint_t);
readline* editor_page_up                    (container*, readline*, wint_t);
readline* editor_goto_beginning_of_document (container*, readline*, wint_t);
readline* editor_goto_end_of_document       (container*, readline*, wint_t);
readline* editor_page_center_cursor         (container*, readline*, wint_t);
readline* editor_kill_to_end_of_line        (container*, readline*, readline*);
readline* editor_kill_to_beginning_of_line  (container*, readline*, readline*);
readline* editor_yank_line                  (container*, readline*, readline*);

#endif /* EDITOR_GUARD */
