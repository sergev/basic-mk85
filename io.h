#ifndef _IO_H_
#define _IO_H_

/* function prototypes */

int keyb0770 (void);
int keyb0820 (void);
void display_char (unsigned int code);
void lcd_char (unsigned int code);
void clear_screen (void);
void display_7seg (unsigned int n);
int display_string (unsigned int addr);
int keys_stop_exe_ac (void);
void display_short_string (unsigned int addr);

#endif
