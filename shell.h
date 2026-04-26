#ifndef SHELL_H
#define SHELL_H

void shell_init();
void shell_process_char(char c);
void shell_execute(char* cmd, int len);

#endif