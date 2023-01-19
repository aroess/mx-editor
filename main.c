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


#include <locale.h>
#include <signal.h>
#include "editor.h"

char WIN_RESIZED = FALSE;
void win_resize_handler(int sig) {
    WIN_RESIZED = TRUE;
    signal(SIGWINCH, win_resize_handler);
}

readline *handle_arrow_keys(container *con, readline *row_pointer)
{
    wint_t unichar = getwchar();
    switch (unichar) {
        case 'A':
            return editor_move_previous_line(con, row_pointer, unichar);
        case 'B':
            return editor_move_next_line(con, row_pointer, unichar);
        case 'C':
            return editor_forward_char(con, row_pointer, unichar);
        case 'D':
            return editor_backward_char(con, row_pointer, unichar);
    }
    return row_pointer;
}

readline *handle_goto(container *con, readline *row_pointer,
                      readline *minibuffer_pointer)
{
    if (con->minibuffer_mode) return row_pointer;
    infobar_print(con, "GOTO LINE:");
    make_new_row(minibuffer_pointer);
    con->temp_row = con->current_row;
    activate_minibuffer(con, minibuffer_pointer, 11);
    row_pointer = minibuffer_pointer;
    con->minibuffer_mode = TRUE;
    return row_pointer;
}

readline *handle_save (container *con, readline *row_pointer,
                      readline *minibuffer_pointer)
{
    if (con->minibuffer_mode) return row_pointer;
    if (con->buffer_filename != NULL) {
        editor_save_file(con, con->buffer_filename);
        return row_pointer;
    } else {
        infobar_print(con, "FILENAME:");
        make_new_row(minibuffer_pointer);
        con->temp_row = con->current_row;
        activate_minibuffer(con, minibuffer_pointer, 10);
        row_pointer = minibuffer_pointer;
        con->minibuffer_mode = TRUE;
        return row_pointer;
    }
}

readline *handle_search_forward (container *con, readline *row_pointer,
                      readline *minibuffer_pointer)
{
    if (con->minibuffer_mode) return row_pointer;
    infobar_print(con, "SEARCH FORWARD:");
    make_new_row(minibuffer_pointer);
    activate_minibuffer(con, minibuffer_pointer, 16);
    return minibuffer_pointer;
}

readline *handle_cancel (container *con, readline *row_pointer,
                         readline *minibuffer_pointer)
{
    if (con->minibuffer_mode) {
        deactivate_minibuffer(con, row_pointer);
        free(minibuffer_pointer->buffer);
        infobar_print(con, "Quit\0");
        return &(con->rows[con->current_row]);
    }
    return row_pointer;
}

int main (int argc, char *argv[]) {

    if(argc > 1 && access(argv[1], R_OK) == -1)
        die("Could not read input file");

    setlocale (LC_ALL, "");

    /* save terminal terminal parameters */
    static struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    /*  set terminal to raw mode
     *  ICANON    input is not buffered
     *  ECHO      don't write input to screen
     *  ISIG      ignore control signals
     *  VMIN      number of characters to read
     *  VTIME     wait indefinitely
     */
    newt.c_iflag&= ~(ISTRIP | INLCR | IXON | IXANY | IXOFF);
    newt.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ANSI_RESET_SCREEN;

    wint_t unichar;        /* holds multibyte characters */
    container con;         /* container that keeps tracks of readlines */
    readline *row_pointer; /* pointer to current readline */

    char ctrl_x_modifier = FALSE;
    char alt_modifier    = FALSE;

    char message[MINIBUFFER_LIMIT];
    memset(message, 0, MINIBUFFER_LIMIT);

    /* array of function pointers of return type readline*
     * CTRL + 'a' = -96 + 97 = 1
     * array index 0 will be not used! */
    readline* (*keybinding_ctrl[27])(container*, readline*, wint_t);
    readline* (*keybinding_alt [27])(container*, readline*, wint_t);

    /* init with NULL pointers */
    for (short i = 0; i < 27; i++) {
        keybinding_ctrl[i] = NULL;
        keybinding_alt [i] = NULL;
    }

    /* define keybindings */
    keybinding_ctrl[KEY_CTRL + 'a'] = editor_move_beginning_of_line;
    keybinding_ctrl[KEY_CTRL + 'e'] = editor_move_end_of_line;
    keybinding_ctrl[KEY_CTRL + 'd'] = editor_delete_forward_char;
    keybinding_ctrl[KEY_CTRL + 'f'] = editor_forward_char;
    keybinding_ctrl[KEY_CTRL + 'b'] = editor_backward_char;
    keybinding_ctrl[KEY_CTRL + 'n'] = editor_move_next_line;
    keybinding_ctrl[KEY_CTRL + 'p'] = editor_move_previous_line;
    keybinding_ctrl[KEY_CTRL + 'v'] = editor_page_down;
    keybinding_ctrl[KEY_CTRL + 'l'] = editor_page_center_cursor;
    keybinding_alt [KEY_CTRL + 'f'] = editor_forward_word;
    keybinding_alt [KEY_CTRL + 'b'] = editor_backward_word;
    keybinding_alt [KEY_CTRL + 'v'] = editor_page_up;

    /* init container */
    con.current_row     = 0;
    con.max_row         = 1;
    con.hpadding        = 0;
    con.vpadding        = 0;
    con.row_length      = ROW_BLOCK_SIZE;
    con.rows            = malloc(sizeof(struct readline) * ROW_BLOCK_SIZE);
    con.minibuffer_mode = FALSE;
    con.temp_row        = 0;
    con.buffer_filename = NULL;

    /* init yank line */
    readline  yank_line;
    readline *yank_line_pointer = &yank_line;
    yank_line_pointer->buffer   = NULL;

    /* init minibuffer */
    readline  minibuffer;
    readline *minibuffer_pointer = &minibuffer;
    int       func_id;

    /* array of function pointers to minibuffer callback functions */
    void (*minibuffer_callback[3])(container*, char[]);
    minibuffer_callback[GOTO_FUNC]    = editor_goto_line;
    minibuffer_callback[SAVE_FUNC]    = editor_save_file;
    minibuffer_callback[SEARCHF_FUNC] = editor_search_forward;

    /* read input file */
    if (argc > 1) {
        con.buffer_filename = strdup(argv[1]);
        editor_load_file(&con, argv[1]);
        con.current_row = 0;
        row_pointer = &con.rows[con.current_row];
    } else {
        row_pointer = &con.rows[con.current_row];
        make_new_row(row_pointer);
    }

    infobar_print(&con, "Welcome to mx! Press C-x C-c to quit.\0");
    signal(SIGWINCH, win_resize_handler);

    /* main loop */
    while (1) {
        unichar = getwchar();
        if (WIN_RESIZED) {
            editor_page_center_cursor(&con, row_pointer, unichar);
            if (con.minibuffer_mode)
                minibuffer_redraw(&con, row_pointer);
            WIN_RESIZED = FALSE;
        }
        if (alt_modifier) {
            switch (unichar) {
                case BRACKETLEFT:
                    row_pointer = handle_arrow_keys(&con, row_pointer);
                    break;
                case '<':
                    row_pointer = editor_goto_beginning_of_document(
                            &con, row_pointer, unichar);
                    break;
                case '>':
                    row_pointer = editor_goto_end_of_document(
                            &con, row_pointer, unichar);
                    break;
                case 'd':
                    while (BUFFER[CURSOR] != 32 && CURSOR != LINE_END ) {
                        row_pointer = editor_delete_forward_char(
                                &con, row_pointer, unichar);
                    }
                    break;
                case 'g':
                    row_pointer = handle_goto(&con, row_pointer,
                                              minibuffer_pointer);
                    func_id = GOTO_FUNC;
                    break;
                default:
                    unichar = KEY_CTRL + unichar;
                    if ((unichar > 0) && (unichar <= 26)) {
                        if (keybinding_alt[unichar] != NULL)
                            row_pointer = (*keybinding_alt[unichar])(
                                    &con, row_pointer, unichar);
                    } else {
                        infobar_print(&con, "unknown keybinding\0");
                    }
            }
            alt_modifier = FALSE;
            continue;
        }
        if (con.minibuffer_mode == FALSE) infobar_erase(&con);
        if (ctrl_x_modifier) {
            switch (unichar) {
                case KEY_CTRL + 'c':
                    infobar_print(&con, "Really quit? (y/n)\0");
                    while ((unichar = getwchar())) {
                        if (unichar == 'y') goto QUIT;
                        if (unichar == 'n') { infobar_erase(&con); break; }
                    }
                    break;
                case KEY_CTRL + 's':
                    row_pointer = handle_save(&con, row_pointer,
                                              minibuffer_pointer);
                    func_id = SAVE_FUNC;
                    break;
                case '=':
                    infobar_print_position(&con);
                    break;
                default:
                    infobar_print(&con, "unknown keybinding\0");
            }
            ctrl_x_modifier = FALSE;
            continue;
        }
        switch (unichar) {
            case KEY_ALT:
                alt_modifier = TRUE;
                break;
            case KEY_CTRL + 'x':
                if (con.minibuffer_mode) break;
                ctrl_x_modifier = TRUE;
                infobar_print(&con, "C-x\0");
                break;
            case KEY_BACKSPACE:
                row_pointer = editor_delete_char(&con, row_pointer, unichar);
                break;
            case KEY_ENTER:
                if (con.minibuffer_mode) {
                    deactivate_minibuffer(&con, row_pointer);
                    if (minibuffer_pointer->line_end > MINIBUFFER_LIMIT) {
                        infobar_print(&con, "ERROR: Minibuffer overflow\0");
                        break;
                    }
                    sprintf(
                        message,
                        "%ls",
                        &minibuffer_pointer->buffer[minibuffer_pointer->margin]
                    );
                    (*minibuffer_callback[func_id])(&con, message);
                    row_pointer = &con.rows[con.current_row];
                    free(minibuffer_pointer->buffer);
                } else {
                    row_pointer = editor_newline(&con, row_pointer);
                }
                break;
            case KEY_CTRL + 'g':
                row_pointer = handle_cancel(&con, row_pointer,
                                            minibuffer_pointer);
                break;
            case KEY_TAB:
                editor_insert_tab(&con, row_pointer);
                break;
            case KEY_CTRL + 'k':
                yank_line_pointer = editor_kill_to_end_of_line(
                        &con, row_pointer, yank_line_pointer);
                break;
            case KEY_CTRL + 'u':
                yank_line_pointer = editor_kill_to_beginning_of_line(
                        &con, row_pointer, yank_line_pointer);
                break;
            case KEY_CTRL + 'y':
                row_pointer = editor_yank_line(
                        &con, row_pointer, yank_line_pointer);
                break;
            case KEY_CTRL + 's':
                row_pointer = handle_search_forward(&con, row_pointer,
                                                    minibuffer_pointer);
                func_id = SEARCHF_FUNC;
            default:
                /* keybindings with ctrl modifier */
                if ((unichar > 0) && (unichar <= 26)) {
                    if (keybinding_ctrl[unichar] != NULL) {
                        row_pointer = (*keybinding_ctrl[unichar])(
                                &con, row_pointer, unichar);
                    } else {
                        if (!con.minibuffer_mode)
                            infobar_print(&con, "unknown keybinding\0");
                    }
                    continue;
                }
                /* no modifier */
                editor_insert_char(&con, row_pointer, unichar);
        }
    }

    QUIT:
    ANSI_RESET_SCREEN;
    /* restore terminal settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (DEBUG) {
        for (short i = 0; i < LINE_LEN; i++) printf("%lc", BUFFER[i]);
        printf("\n");
        for (short i = 0; i < LINE_LEN; i++) printf("%d ", BUFFER[i]);
        printf("\n");
        printf("current_row = %d\n", con.current_row);
        printf("max_row = %d\n", con.max_row);
        printf("line_end = %d\n", LINE_END);
        printf("cursor = %d\n", CURSOR);
        printf("hpadding = %d\n", con.hpadding);
        printf("vpadding = %d\n", con.vpadding);
        /* printf("win width %d\n",  w.ws_col); */
        /* printf("win height %d\n",  w.ws_row);     */
        printf("row_length %d\n",  con.row_length);
        printf("margin %d\n",  MARGIN);
        printf("temp_row %d\n",  con.temp_row);
        printf("buffer_filename %s\n",  con.buffer_filename);
    }
    return 0;
}

/* Emacs indentation: 
 (progn 
  (c-guess) 
  (setq indent-tabs-mode nil)
  (c-set-offset 'case-label '+)) */
