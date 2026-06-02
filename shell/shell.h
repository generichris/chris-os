#ifndef SHELL_H
#define SHELL_H

void shell_init(void);
void shell_process_char(char c);
void shell_execute(char* cmd, int len);
void shell_cursor_left(void);
void shell_cursor_right(void);
void shell_history_up(void);
void shell_history_down(void);

#endif