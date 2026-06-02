#ifndef INSTALLER_H
#define INSTALLER_H



#define INSTALL_SECTOR_COUNT 53248

void installer_start();
void installer_handle_char(char c);
int  installer_is_active();

#endif