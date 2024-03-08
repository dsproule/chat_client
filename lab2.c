/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Donovan Sproule (das2313)
 *	     Claire Cizdziel (ctc2156)
 * 	     Nicolas Alarcon (na2946)		
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

/* ****************************************************** */
#include "util.h"

#define CLEAR_SCREEN() clear_section(0, MAX_ROW + 1, 0, MAX_COL + 1);
#define CLEAR_OUTPUT() clear_section(0, MAX_ROW - 2, 0, MAX_COL + 1);
#define CLEAR_INPUT()  clear_section(MAX_ROW - 1, MAX_ROW + 1, 0, MAX_COL + 1);
#define DRAW_CHAR(CHAR) fbputchar((char)CHAR, (MAX_ROW - 1 + (cursor >= MAX_COL)), cursor % MAX_COL);

#define MAX_BUFF MAX_COL * 2
int output_line;

/* ****************************************************** */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('*', 23, col);
  }

  fbputs("Hello CSEE 4840 World!", 4, 10);

  /* Open the keyboard */
  
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }
  
    
  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* ********* Init screen ********** */
 // CLEAR_SCREEN();
  for (int col = 0; col < MAX_COL + 1; col++)
	  fbputs("*", MAX_ROW - 2, col);

  // move these to top when everything works
  char msg_buff[MAX_BUFF], cur_c;
  int cursor, msg_len, orig_curs, i;
  unsigned int pressed;

  cur_c = '_';
  msg_len = cursor = output_line = 0;
  pressed = 0;

  /* Look for and handle keypresses */
  CLEAR_OUTPUT();
  for (cursor = 0; cursor < MAX_BUFF; cursor++)
      msg_buff[cursor] = ' ';
  cursor = 0;

  for (;;) {
	cur_c = (cur_c == '_') ? msg_buff[cursor] : '_';
	fbputchar(cur_c, (MAX_ROW - 1 + (cursor >= MAX_COL)), cursor % MAX_COL);
    	  
	libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 500);

	/* Finds the letter flags with (doesn't work for capitals yet)*/
	for (i = 0; i < 26; i++)
		if ((pressed >> i) & 0b1) {
			DRAW_CHAR((char)(97 + i));
			msg_buff[cursor++] = (char)(97 + i);
			msg_len++;
		}

	if (transferred == sizeof(packet)) {
		pressed = 0;
		for (i = 0; i < 5; i++) {
			if (packet.keycode[i] == 0)
				continue;

			sprintf(keystate, "%c", packet.keycode[i] + 93);
			if (keystate[0] == 137)
				keystate[0] = ' ';
			printf("%d\n", packet.keycode[i]);
		
			if (packet.modifiers && (USB_LSHIFT | USB_RSHIFT))
				keystate[0] -= 32;

			if (packet.keycode[i] == 0x29) {						// ESC
				CLEAR_INPUT();
				exit(0);
			} else if (packet.keycode[i] == 0x4f && cursor < msg_len) {			// RIGHT
				DRAW_CHAR(msg_buff[cursor]);
				cursor++;
			} else if (packet.keycode[i] == 0x50 && cursor > 0) {				// LEFT
				DRAW_CHAR(msg_buff[cursor]);
				cursor--;
			// may be redundant if-conditions
			} else if (packet.keycode[i] == 0x2a && cursor > 0 && msg_len > 0) {		// BACKSPACE
				msg_buff[msg_len+1] = ' ';
				orig_curs = cursor;
				msg_len--;
				if (cursor < msg_len) {
					for (; msg_buff[cursor] != ' '; cursor++) {
						msg_buff[cursor] = msg_buff[cursor + 1];
						DRAW_CHAR(msg_buff[cursor]);
					}
					cursor = orig_curs;
				} else {
					msg_buff[cursor] = ' ';
					cursor--;
					fbputchar(' ', (MAX_ROW - 1 + (cursor >= MAX_COL)), cursor % MAX_COL);
				}
			} else if (packet.keycode[i] == 0x28) {						// ENTER
				msg_buff[msg_len] = '\0';
				printf("MESSAGE: %s\n", msg_buff);
				// dont forget to look for errors
				write(sockfd, msg_buff, msg_len);
			
				CLEAR_INPUT();
				for (cursor = 0; cursor < MAX_BUFF; cursor++)
					msg_buff[cursor] = ' ';
				cursor = 0;
				msg_len = 0;
			} else if (cursor != MAX_BUFF) {
				if (packet.keycode[i] != 0x4f)
					pressed |= (0b1 << (keystate[0] - 97));
			}
		}
	
	}
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    if (output_line == MAX_ROW - 2) {
	output_line = 0;
	CLEAR_OUTPUT();
    }	
    for (i = 0; recvBuf[i] != '\0'; i++) {
	if (recvBuf[i] != '\n')
		fbputchar(recvBuf[i], output_line, i % MAX_COL);
	if (i == MAX_COL - 1)
		output_line++;
    }

   output_line++;
  }

  return NULL;
}

void clear_section(int row_start, int row_end, int col_start, int col_end)
{
	int row, col;

	for (row = row_start; row < row_end; row++)
		for (col = col_start; col < col_end; col++)
			fbputs(" ", row, col);
}
