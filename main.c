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


#include <locale.h>
#include <signal.h>
#include "editor.h"

char WIN_RESIZED = FALSE;
void win_resize_handler(int sig) {
    WIN_RESIZED = TRUE;
    signal(SIGWINCH, win_resize_handler);
}

int main (int argc, char *argv[]) {
    
    if (argc < 2)
        die("No file to read"); 
    if(access(argv[1], R_OK) == -1)
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
    
    char status_message[80];
    memset(status_message, 0, 80);
    
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
    con.current_row = 0;
    con.max_row     = 1;
    con.hpadding    = 0;
    con.vpadding    = 0;
    con.row_length  = ROW_BLOCK_SIZE;
    con.rows        = malloc(sizeof(struct readline) * ROW_BLOCK_SIZE);

    readline yank_line;
    readline *yank_line_pointer = &yank_line;
    yank_line_pointer->buffer = NULL;
    
    /* read input file */
    editor_load_file(&con, argv[1]);
    con.current_row = 0; 
    row_pointer = &con.rows[con.current_row];

    infobar_print(&con, "Welcome to mx! Press C-x C-c to quit.\0"); 
    signal(SIGWINCH, win_resize_handler);
    
    /* main loop */
    while ((unichar = getwchar())) {     
        if (WIN_RESIZED) {
            screen_redraw(&con, WHOLE);
            WIN_RESIZED = FALSE;
        }  
        if (alt_modifier) {
            switch (unichar) {
                case '<':
                    row_pointer = editor_goto_beginning_of_document(&con, row_pointer, unichar);
                    break;
                case '>':
                    row_pointer = editor_goto_end_of_document(&con, row_pointer, unichar);
                    break;             
                default:
                    unichar = KEY_CTRL + unichar;
                    if ((unichar > 0) && (unichar <= 26)) {
                        if (keybinding_alt[unichar] != NULL) 
                            row_pointer = (*keybinding_alt[unichar])(&con, row_pointer, unichar);
                    } else {
                        infobar_print(&con, "unknown keybinding\0");
                    }
            }
            alt_modifier = FALSE;
            continue;
        }
        infobar_erase(&con);
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
                    editor_save_file(&con, argv[1]);
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
                ctrl_x_modifier = TRUE;
                infobar_print(&con, "C-x\0");
                break;
            case KEY_BACKSPACE:
                row_pointer = editor_delete_char(&con, row_pointer, unichar);
                break;
            case KEY_ENTER:
                row_pointer = editor_newline(&con, row_pointer);
                break;
            case KEY_TAB:
                editor_insert_tab(&con, row_pointer);
                break;
            case KEY_CTRL + 'k':
                yank_line_pointer = editor_kill_to_end_of_line(&con, row_pointer, yank_line_pointer);
                break;
            case KEY_CTRL + 'u':
                yank_line_pointer = editor_kill_to_beginning_of_line(&con, row_pointer, yank_line_pointer);
                break;
            case KEY_CTRL + 'y':
                row_pointer = editor_yank_line(&con, row_pointer, yank_line_pointer);
                break;
            default:
                /* keybindings with ctrl modifier */
                if ((unichar > 0) && (unichar <= 26)) {
                    if (keybinding_ctrl[unichar] != NULL) 
                        row_pointer = (*keybinding_ctrl[unichar])(&con, row_pointer, unichar);
                    else
                        infobar_print(&con, "unknown keybinding\0");
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
        printf("win width %d\n",  w.ws_col);
        printf("win height %d\n",  w.ws_row);    
        printf("row_length %d\n",  con.row_length);
        printf("yank line: ");
        for (short i = 0; i < yank_line_pointer->line_end; i++) printf("%d ", yank_line_pointer->buffer[i]);
        printf("\n");
    }     
    return 0;
}
