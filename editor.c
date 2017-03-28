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
 *  along with mx.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "editor.h"

void die(const char *message) {
    if(errno) {
        perror(message);
    } else {
        printf("ERROR: %s\n", message);
    }
    exit(EXIT_FAILURE);
}

int get_window_width() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

int get_window_height() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}

void make_new_row(readline *row_pointer) {
    CURSOR   = 0;
    LINE_END = 0;
    MARGIN   = 0;
    LINE_LEN = LINE_BLOCK_SIZE;
    BUFFER   = calloc(LINE_LEN, sizeof(wint_t));
}

void extend_row(readline *row_pointer) {
    LINE_LEN += LINE_BLOCK_SIZE;
    BUFFER = realloc(BUFFER, sizeof(wint_t) * LINE_LEN);
    for (int i = LINE_END; i < LINE_LEN + 0; i++)
        BUFFER[i] = 0;
}
void extend_container(container *con) {
    con->row_length += ROW_BLOCK_SIZE;
    con->rows = realloc(con->rows, sizeof(struct readline) * con->row_length);
}

/*-----------------------------------------------  
    low level functions altering the screen
 -----------------------------------------------*/

void screen_set_cursor(int row, int cursor, int hpadding, int vpadding) {
    int real_cursor = cursor-hpadding+1;
    printf("\033[%d;%df", row-vpadding+1, real_cursor < 1 ? 1 : real_cursor);
}

void screen_set_char(int row, int cursor, wint_t c, int hpadding, int vpadding) {
    screen_set_cursor(row, cursor, hpadding, vpadding);
    printf("%lc", c);
}

void screen_shift_region_right(wint_t buffer[], int row, int cursor, int line_end, int hpadding, int vpadding) {
    for(int i = cursor + 1; i < line_end; i++)
        screen_set_char(row, i, buffer[i], hpadding, vpadding);
}

void screen_shift_region_left(wint_t buffer[], int row, int cursor, int line_end, int hpadding, int vpadding) {
    for(int i = cursor - 1; i < line_end - 1; i++)
        screen_set_char(row, i, buffer[i], hpadding, vpadding);
}

void screen_redraw(container *con, enum draw_mode mode) {
    int start;
    int max = MAX_ROW;
    switch (mode) {
        case WHOLE: start = VPADDING;  break;
        case REGION_DOWN: start = CUR_ROW-1; break;
        case REGION_UP: start = CUR_ROW;   break;
        case LINE: start = CUR_ROW; max = CUR_ROW+1; break;
    }
    /* Only draw visible rows. Note: redrawing to window_height
     * causes the terminal screen to scroll down (CR in final line)! */
    if (MAX_ROW >= get_window_height())
        max = get_window_height() - 1 + VPADDING;
    /* Don't read unallocated line buffers */
    if (max > MAX_ROW)
        max = MAX_ROW;
    /* Make sure the whole tab is printed on screen */
    readline *row_pointer = &con->rows[con->current_row];
    if (HPADDING) 
        HPADDING = (CURSOR/TAB_STOP_WIDTH) * TAB_STOP_WIDTH;
    if (LINE_END + HPADDING >= LINE_LEN) extend_row(row_pointer);
    if (mode == WHOLE) ANSI_RESET_SCREEN;
    for (int i = start; i < max; i++) {
        screen_set_cursor(i, 0, HPADDING, VPADDING);
        ANSI_KILL_LINE;
        if(con->rows[i].buffer != NULL) {
            for (int j = 0; j < get_window_width()-1; j++) {
                if (j >= con->rows[i].line_end-HPADDING) break;
                printf("%lc", con->rows[i].buffer[j+HPADDING]);
            }
        }
    }
    /* erase last line */
    screen_set_cursor(MAX_ROW, 0, HPADDING, VPADDING);
    ANSI_KILL_LINE;
}

void minibuffer_redraw(container *con, readline *row_pointer) {
    screen_set_cursor(get_window_height()-1, MARGIN, 0, 0);
    ANSI_KILL_LINE;
    for (int j = MARGIN; j < get_window_width()-1; j++) {
        if (j >= LINE_END-HPADDING) break;
        printf("%lc", BUFFER[j+HPADDING]);
    }
   
}

/*-----------------------------------------------  
    low level functions altering the buffer
 -----------------------------------------------*/

void buffer_set_char(wint_t buffer[], int cursor, wint_t c) {
    buffer[cursor] = c;
}

void buffer_shift_region_right(wint_t buffer[], int row, int cursor, int line_end) {
    memmove(
            &buffer[cursor+1],
            &buffer[cursor],
            (line_end-cursor) * sizeof(wint_t)
            );
}

void buffer_shift_region_left(wint_t buffer[], int row, int cursor, int line_end) {
    memmove(
            &buffer[cursor-1],
            &buffer[cursor],
            (line_end-cursor) * sizeof(wint_t)
            );
}

char buffer_is_space(wint_t buffer[], int cursor) {
    return (buffer[cursor] == 32) ? TRUE : FALSE;
}

void buffer_shift_line_down(container *con) {
    memmove(
            &con->rows[con->current_row+1],
            &con->rows[con->current_row],
            (con->max_row - con->current_row) * sizeof(readline)
            );
}
void buffer_shift_line_up(container *con) {
    memmove(
            &con->rows[con->current_row],
            &con->rows[con->current_row+1],
            (con->max_row - con->current_row) * sizeof(readline)
            );
}

/*-----------------------------------------------  
    high level editor functions altering the buffer
 -----------------------------------------------*/

readline *editor_newline(container *con, readline *row_pointer) {
    buffer_shift_line_down(con);
    if (MAX_ROW + 1 == con->row_length) extend_container(con);
    CUR_ROW++;
    /* two cols pointing to same buffer! */
    con->rows[CUR_ROW].buffer = NULL; 
    row_pointer = &con->rows[CUR_ROW];
    make_new_row(row_pointer);
    MAX_ROW++;
    readline *row_pointer_prev;
    row_pointer_prev = &con->rows[CUR_ROW-1];
    /* make sure it fits */
    while (row_pointer->line_length < LINE_END_PREV + LINE_END)
        extend_row(row_pointer);
    /* copy down from line above */
    for (int i = 0; i <= (LINE_END_PREV - CURSOR_PREV); i++) {
        BUFFER[i] = BUFFER_PREV[CURSOR_PREV + i];
        BUFFER_PREV[CURSOR_PREV + i]  = 0;
    }
    LINE_END = LINE_END_PREV - CURSOR_PREV;
    LINE_END_PREV = CURSOR_PREV;
    buffer_set_char(BUFFER_PREV, CURSOR_PREV,  0xA);

    char redraw = FALSE;
    if(HPADDING) {
        HPADDING = 0;
        redraw = TRUE;
    }
    if(CUR_ROW - VPADDING == get_window_height() - 1) {
        VPADDING++;
        redraw = TRUE;
    }
    redraw ? screen_redraw(con, WHOLE) : screen_redraw(con, REGION_DOWN);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_insert_char(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode && unichar == 0xA) return row_pointer;
    /* with the current temios settings a window resize inserts the char -1, ignore this */
    if (unichar == -1) return row_pointer;
    /* If a tab character is encountered fill space until tab stop is reached */
    if (BUFFER[CURSOR] == 0x9) {
        BUFFER[CURSOR] = unichar;
        CURSOR++;
        int current_tab_stop = (CURSOR/TAB_STOP_WIDTH) * TAB_STOP_WIDTH;
        if (CURSOR == current_tab_stop) {
            editor_insert_tab(con, row_pointer);
            CURSOR = current_tab_stop;
        } else {
            BUFFER[CURSOR] = 0x9;
            screen_redraw(con, LINE);
        }
        screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
        return row_pointer;
    }
    LINE_END++;
    if (LINE_END >= LINE_LEN) extend_row(row_pointer);
    buffer_shift_region_right(BUFFER, CUR_ROW, CURSOR, LINE_END);
    buffer_set_char(BUFFER, CURSOR, unichar);
    if (CURSOR-HPADDING >= get_window_width() - 1) { 
        HPADDING++;
        if (con->minibuffer_mode)
            minibuffer_redraw(con, row_pointer);
        else 
            screen_redraw(con, WHOLE);
    } else {
        screen_shift_region_right(BUFFER, CUR_ROW, CURSOR, LINE_END, HPADDING, VPADDING);
        screen_set_char(CUR_ROW, CURSOR, unichar, HPADDING, VPADDING);
    }
    CURSOR++;
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_insert_tab(container *con, readline *row_pointer) {
    if (con->minibuffer_mode) return row_pointer;
    /* insert tab character, then fill buffer with TAB_PAD_CHAR until
     * next tab stop is reached  */
    int next_tab_stop = (CURSOR/TAB_STOP_WIDTH) * TAB_STOP_WIDTH + TAB_STOP_WIDTH;
    editor_insert_char(con, row_pointer, KEY_TAB);
    for(int i = CURSOR; i < next_tab_stop; i++)
        editor_insert_char(con, row_pointer, TAB_PAD_CHAR);
    screen_redraw(con, LINE);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_delete_char(container *con, readline *row_pointer, wint_t unichar) {
    if (CURSOR == MARGIN && con->minibuffer_mode) return row_pointer;
    if (CURSOR == 0) return editor_delete_line(con, row_pointer, unichar);
    char redraw = FALSE;
    if (BUFFER[CURSOR-1] == TAB_PAD_CHAR) {
        while (BUFFER[CURSOR-1] == TAB_PAD_CHAR) {
            buffer_shift_region_left(BUFFER, CUR_ROW, CURSOR, LINE_END+1);
            buffer_set_char(BUFFER, LINE_END, 0);
            CURSOR--;
            LINE_END--;
        }
        redraw = TRUE;
    }
    buffer_shift_region_left(BUFFER, CUR_ROW, CURSOR, LINE_END+1);
    buffer_set_char(BUFFER, LINE_END, 0);
    if (CURSOR <= HPADDING - 1) {
        HPADDING--;
        screen_redraw(con, WHOLE);
    } else if (redraw) {
        screen_redraw(con, LINE);
    } else {
        screen_shift_region_left(BUFFER, CUR_ROW, CURSOR, LINE_END, HPADDING, VPADDING);
        screen_set_char(CUR_ROW, LINE_END-1, ' ', HPADDING, VPADDING);
    }
    LINE_END--;
    CURSOR--;
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_delete_forward_char(container *con, readline *row_pointer, wint_t unichar) {
    if (CURSOR == LINE_END) return row_pointer;
    if (BUFFER[CURSOR] == 0x9) {
        do {
            buffer_shift_region_left(BUFFER, CUR_ROW, CURSOR, LINE_END+1);
            buffer_set_char(BUFFER, LINE_END, 0);
            LINE_END--;
        } while (BUFFER[CURSOR] == TAB_PAD_CHAR);
        screen_redraw(con, LINE);
        screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
        return row_pointer;
    }
    buffer_shift_region_left(BUFFER, CUR_ROW, CURSOR+1, LINE_END+1);
    screen_shift_region_left(BUFFER, CUR_ROW, CURSOR+1, LINE_END, HPADDING, VPADDING);
    LINE_END--;
    screen_set_char(CUR_ROW, LINE_END, ' ', HPADDING, VPADDING);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_delete_line(container *con, readline *row_pointer, wint_t unichar) {
    if (CUR_ROW == 0) return row_pointer;
    readline *row_pointer_prev;
    row_pointer_prev = &con->rows[CUR_ROW-1];
    /* make sure it fits */
    while (row_pointer_prev->line_length < LINE_END_PREV + LINE_END)
        extend_row(row_pointer_prev);
    /* copy up to line above */
    for (int i = 0; i <= LINE_END; i++) {
        BUFFER_PREV[LINE_END_PREV + i] = BUFFER[i];
    }
    LINE_END_PREV = LINE_END_PREV + LINE_END;
    MAX_ROW--;
    buffer_shift_line_up(con);
    con->rows[MAX_ROW].buffer = NULL;
    con->rows[MAX_ROW].cursor = 0;
    con->rows[MAX_ROW].line_end = 0;
    CUR_ROW--;
    char redraw = FALSE;
    if (CUR_ROW + 1 == VPADDING) {
        VPADDING--;
        redraw = TRUE;
    }
    redraw ? screen_redraw(con, WHOLE) : screen_redraw(con, REGION_UP);
    row_pointer = &con->rows[CUR_ROW];
    CURSOR = 0; /* no need to reset HPADDING, function can only called if HPADDING = 0 */
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline *editor_kill_to_end_of_line(container *con, readline *row_pointer, readline *yank_line_pointer) {
    if (con->minibuffer_mode) return row_pointer;
    if (yank_line_pointer->buffer) free(yank_line_pointer->buffer);
    yank_line_pointer->buffer = malloc((LINE_END - CURSOR) * sizeof(wint_t));
    yank_line_pointer->line_end = 0; 
    for (int i = 0, k = 0; i < (LINE_END - CURSOR); i++) {
        if (BUFFER[CURSOR + i] != TAB_PAD_CHAR) {
            yank_line_pointer->buffer[k] = BUFFER[CURSOR + i];
            yank_line_pointer->line_end++;
            k++;
        }
        BUFFER[CURSOR + i] = 0;
        screen_set_char(CUR_ROW, CURSOR + i, ' ', HPADDING, VPADDING); 
    }
    BUFFER[CURSOR] = BUFFER[LINE_END]; /* copy CR or 0 */
    BUFFER[LINE_END] = 0;
    LINE_END = CURSOR;
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return yank_line_pointer;
}

readline *editor_kill_to_beginning_of_line(container *con, readline *row_pointer, readline *yank_line_pointer) {
    if (con->minibuffer_mode) return row_pointer;
    if (CURSOR == 0) return yank_line_pointer;
    if (yank_line_pointer->buffer) free(yank_line_pointer->buffer);
    yank_line_pointer->buffer = malloc(CURSOR * sizeof(wint_t));
    yank_line_pointer->line_end = 0;
    /* copy to yank line */
    for (int i = 0, k = 0; i < CURSOR; i++) {
        if (BUFFER[i] != TAB_PAD_CHAR) {
            yank_line_pointer->buffer[k] = BUFFER[i];
            yank_line_pointer->line_end++;
            k++;
        }
    }
    /* move values right from cursor to line start */
    for (int i = CURSOR, k = 0; i < LINE_END+1; i++, k++)
        BUFFER[k] = BUFFER[i];
    /* delete excess characters */
    for (int i = LINE_END - CURSOR + 1 ; i <= LINE_END; i++)
        BUFFER[i] = 0;
    LINE_END -= CURSOR;
    CURSOR = 0;
    screen_redraw(con, LINE);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return yank_line_pointer;
}

readline *editor_yank_line(container *con, readline *row_pointer, readline *yank_line_pointer) {
    if (con->minibuffer_mode) return row_pointer;
    for (int i = 0; i < yank_line_pointer->line_end; i++) {
        if (yank_line_pointer->buffer[i] == 0x9)
            editor_insert_tab(con, row_pointer);
        else
            editor_insert_char(con, row_pointer, yank_line_pointer->buffer[i]);
    }
    return row_pointer;
}

/*-----------------------------------------------  
    high level editor functions: movement
 -----------------------------------------------*/

readline* editor_forward_char(container *con, readline *row_pointer, wint_t unichar) {
    if (CURSOR < LINE_END) {
        do {
            CURSOR++;
            if (CURSOR-HPADDING >= get_window_width() - 1) {
                HPADDING++;
                if (con->minibuffer_mode)
                    minibuffer_redraw(con, row_pointer);
                else
                    screen_redraw(con, WHOLE);
            }
            screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
        } while (BUFFER[CURSOR] == TAB_PAD_CHAR);
    }
    return row_pointer;
}

readline* editor_backward_char(container *con, readline *row_pointer, wint_t unichar) {
    if (CURSOR > MARGIN) {
        do {
            CURSOR--;
            if (CURSOR-MARGIN <= HPADDING - 1) {
                HPADDING--;
                if (con->minibuffer_mode)
                    minibuffer_redraw(con, row_pointer);
                else
                    screen_redraw(con, WHOLE);
            }
            screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
        } while (BUFFER[CURSOR] == TAB_PAD_CHAR);
    }
    return row_pointer;
}

readline* editor_forward_word(container *con, readline *row_pointer, wint_t unichar) {
    START:
    if (!buffer_is_space(BUFFER, CURSOR)) { 
        while(!buffer_is_space(BUFFER, CURSOR)) {
            if (CURSOR == LINE_END) break;
            CURSOR++;
        }
    } else {
        while(buffer_is_space(BUFFER, CURSOR)) { /* skip white space */
            if (CURSOR == LINE_END) break;
            CURSOR++;
        }
        goto START;
    }
    if (CURSOR-HPADDING >= get_window_width() - 1) {
        HPADDING = LINE_END - get_window_width() + 1;
        if (con->minibuffer_mode)
            minibuffer_redraw(con, row_pointer);
        else
            screen_redraw(con, WHOLE);
    } 
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_backward_word(container *con, readline *row_pointer, wint_t unichar) {
    if (CURSOR == LINE_END) CURSOR--;
    START:
    if (!buffer_is_space(BUFFER, CURSOR)) {
        while (!buffer_is_space(BUFFER, CURSOR)) { 
            if (CURSOR == MARGIN) break;
            CURSOR--;
        }
    } else {
        while(buffer_is_space(BUFFER, CURSOR)) {
            if (CURSOR == MARGIN) break;
            CURSOR--;
        }
        if (CURSOR != MARGIN) goto START;
    }
    if (CURSOR-MARGIN <= HPADDING - 1) {
        HPADDING--;
        if (con->minibuffer_mode) {
            CURSOR = MARGIN+HPADDING; /* not as expected */
            minibuffer_redraw(con, row_pointer);
        } else {
            screen_redraw(con, WHOLE);
        }
    }
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_move_beginning_of_line(container *con, readline *row_pointer, wint_t unichar) {
    CURSOR = MARGIN;
    if(HPADDING) {
        HPADDING = 0;
        if (con->minibuffer_mode)
            minibuffer_redraw(con, row_pointer);
        else
            screen_redraw(con, WHOLE);
    }
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline* editor_move_end_of_line(container *con, readline *row_pointer, wint_t unichar) {
    CURSOR = LINE_END;
    if (LINE_END > get_window_width()) {
        HPADDING = LINE_END - get_window_width() + 1;
        if (con->minibuffer_mode)
            minibuffer_redraw(con, row_pointer);
        else
            screen_redraw(con, WHOLE);      
    }
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline *editor_move_next_line(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode) return row_pointer;
    if (CUR_ROW == MAX_ROW - 1) return row_pointer;
    CUR_ROW++;
    row_pointer = &con->rows[CUR_ROW];
    char redraw = FALSE;
    if (CUR_ROW - VPADDING == get_window_height() - 1) {
        VPADDING++;
        redraw = TRUE;
    }
    if (HPADDING) {
        HPADDING = 0;
        redraw = TRUE;
    }    
    CURSOR = 0;
    if (redraw) editor_page_center_cursor(con, row_pointer, unichar);
    else        screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline *editor_move_previous_line(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode) return row_pointer;
    if (CUR_ROW == 0) return row_pointer;
    CUR_ROW--;
    row_pointer = &con->rows[CUR_ROW];
    char redraw = FALSE;
    if (CUR_ROW + 1 == VPADDING) {
        VPADDING--;
        redraw = TRUE;
    }
    if (HPADDING) {
        HPADDING = 0;
        redraw = TRUE;
    }
    CURSOR = 0;
    if (redraw) editor_page_center_cursor(con, row_pointer, unichar);
    else        screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline *editor_page_down(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode) return row_pointer;
    int next = CUR_ROW + get_window_height() - 1;
    if (next >= MAX_ROW) next = MAX_ROW - 1;
    CUR_ROW = next;
    row_pointer = &con->rows[CUR_ROW];
    VPADDING = next;
    screen_redraw(con, WHOLE);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline *editor_page_up(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode) return row_pointer;
    int next = CUR_ROW - get_window_height() + 1;
    if (next < 0) next = 0;
    CUR_ROW = next;
    row_pointer = &con->rows[CUR_ROW];
    VPADDING = next;
    screen_redraw(con, WHOLE);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline *editor_goto_beginning_of_document(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode) return row_pointer;
    CUR_ROW = 0;
    VPADDING = 0;
    CURSOR = 0;
    row_pointer = &con->rows[CUR_ROW];
    screen_redraw(con, WHOLE);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

readline *editor_goto_end_of_document(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode) return row_pointer;
    CUR_ROW = MAX_ROW - 1;
    VPADDING = CUR_ROW;
    CURSOR = 0;
    row_pointer = &con->rows[CUR_ROW];
    editor_page_center_cursor(con, row_pointer, unichar);
    return row_pointer;
}
readline *editor_page_center_cursor(container *con, readline *row_pointer, wint_t unichar) {
    if (con->minibuffer_mode) return row_pointer;
    VPADDING = CUR_ROW - get_window_height() / 2;
    if (VPADDING < 0) VPADDING = 0;
    screen_redraw(con, WHOLE);
    screen_set_cursor(CUR_ROW, CURSOR, HPADDING, VPADDING);
    return row_pointer;
}

void editor_goto_line(container *con, char message[]) {
    int line_number = strtol(message, NULL, 10);
    if (line_number == 0) return;
    if (line_number < 1)       line_number = 1;
    if (line_number > MAX_ROW) line_number = MAX_ROW;
    con->current_row = line_number - 1;
    editor_page_center_cursor(con, &con->rows[con->current_row], 0);
}

/*-----------------------------------------------  
    file operations
 -----------------------------------------------*/

void editor_save_file(container *con, char filename[]) {
    FILE *write_fp;
    write_fp = fopen(filename, "w, utf-8");
    if(write_fp == NULL) {
        infobar_error(con, "Could not write file");
        return;
    }
    for (int i = 0; i < con->max_row; i++) {
        /* CR ist not in [0, LINE_END] */
        for (int j = 0; j <= con->rows[i].line_end; j++) { 
            if (con->rows[i].buffer[j] == 0) break;
            if (con->rows[i].buffer[j] != TAB_PAD_CHAR)
                fwprintf(write_fp, L"%lc", con->rows[i].buffer[j]);
        }
    }
    fclose(write_fp);
    infobar_print(con, "document saved\0");
}

void editor_load_file(container *con, char filename[]) {
    FILE *read_fp;
    wint_t unichar;
    readline *row_pointer;
    /* file does exist */
    if(access(filename, R_OK) != -1) {
        read_fp = fopen(filename, "r, utf-8");
        if (read_fp == NULL) infobar_error(con, "Could not load file");
        row_pointer = &con->rows[CUR_ROW];
        make_new_row(row_pointer);
    /* file does not exist */
    } else {
        row_pointer = &con->rows[CUR_ROW];
        make_new_row(row_pointer);
        return;
    }
    while ((unichar = fgetwc(read_fp)) != EOF) {
        /* handle carriage return */
        if (unichar == 0xA) {
            BUFFER[CURSOR] = unichar;
            CUR_ROW++;
            MAX_ROW++;
            CURSOR = 0;
            if (MAX_ROW  == con->row_length) extend_container(con);
            row_pointer = &con->rows[CUR_ROW];
            make_new_row(row_pointer);
            continue;
        }
        /* handle tabs */
        if (unichar == 0x9) {
            BUFFER[CURSOR] = unichar;
            CURSOR++;
            LINE_END++;
            int next_tab_stop = ((CURSOR-1)/TAB_STOP_WIDTH) * TAB_STOP_WIDTH + TAB_STOP_WIDTH;
            for(int i = CURSOR; i < next_tab_stop; i++) {
                BUFFER[CURSOR] = TAB_PAD_CHAR;
                CURSOR++;
                LINE_END++;
                if (LINE_END >= LINE_LEN) extend_row(row_pointer);
            }
            continue;
        }
        /* everything else */
        BUFFER[CURSOR] = unichar;
        CURSOR++;
        LINE_END++;
        if (LINE_END >= LINE_LEN) extend_row(row_pointer);
    }
    CURSOR = 0;
    fclose(read_fp);
    screen_redraw(con, WHOLE);
    screen_set_cursor(0,0,0,0);
}

/*-----------------------------------------------  
    infobar functions
 -----------------------------------------------*/

void infobar_print(container *con, char status_message[]) {
    infobar_erase(con);
    screen_set_cursor(get_window_height()-1, 0, 0, 0);
    ANSI_INVERT_COLOR;
    printf("%.*s", get_window_width()-1, status_message);
    ANSI_REVERT_INVERT_COLOR;
    screen_set_cursor(con->current_row, con->rows[con->current_row].cursor,
                      con->hpadding, con->vpadding);
}
void infobar_error(container *con, char status_message[]) {
    screen_set_cursor(get_window_height()-1, 0, 0, 0);
    ANSI_KILL_LINE;
    ANSI_INVERT_COLOR;
    if(errno) printf("ERROR: %.*s", get_window_width()-8, strerror(errno));
    else      printf("ERROR: %.*s", get_window_width()-8, status_message);
    ANSI_REVERT_INVERT_COLOR;
    screen_set_cursor(con->current_row, con->rows[con->current_row].cursor,
                      con->hpadding, con->vpadding);
}
void infobar_print_position(container *con) {
    screen_set_cursor(get_window_height()-1, 0, 0, 0);
    readline *row_pointer = &con->rows[CUR_ROW];
    char message[80];
    sprintf(message, "CHAR: (%lc, %d, 0x%x) ROW: %d COLUMN: %d",
           BUFFER[CURSOR], BUFFER[CURSOR], BUFFER[CURSOR], CUR_ROW+1, CURSOR+1);
    ANSI_KILL_LINE;
    ANSI_INVERT_COLOR;
    printf("%.*s", get_window_width(), message);
    ANSI_REVERT_INVERT_COLOR;
    screen_set_cursor(con->current_row, con->rows[con->current_row].cursor,
                      con->hpadding, con->vpadding);
}

void infobar_erase(container *con) {
    screen_set_cursor(get_window_height(), 0, 0, 0);
    ANSI_KILL_LINE;
    screen_set_cursor(con->current_row, con->rows[con->current_row].cursor,
                      con->hpadding, con->vpadding);
}

/*-----------------------------------------------  
    minibuffer functions
 -----------------------------------------------*/

void activate_minibuffer(container *con, readline *row_pointer, int margin) {
    MARGIN = margin;
    CURSOR = margin;
    LINE_END = margin;
    screen_set_cursor(get_window_height()-1, margin, 0, 0);
    CUR_ROW = get_window_height()-1;
    ANSI_KILL_LINE;
}

void deactivate_minibuffer(container *con, readline *row_pointer, int save_row) {
    CUR_ROW = save_row;
    HPADDING = 0; /* todo */
    infobar_erase(con);
}
