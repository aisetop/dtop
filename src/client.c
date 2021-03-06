#include <stdio.h>  // printf
#include <stdlib.h> // NULL
#include <string.h> // memset
#include <unistd.h> // close
#include <errno.h>  // errno
#include <signal.h> // signal

// ioctl
#include <sys/ioctl.h>

#include <ncurses.h>

// open
#include <sys/stat.h>
#include <fcntl.h>

// getaddrinfo
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "struct.h"
#include "sensor.h"
#include "display.h"
#include "server.h"
#include "client.h"
#include "io.h"

int stop_client = 0;
int current_machine = 0;

static void handle_standard(int sock)
{
  message_client_t msg_client;
  message_server_t msg_server;

  int ret = 0;
    
  // monitoring
  machine_info_t *m = sensor();

  // First time server don't update, i don't know why...
  for (int i = 0; i < 2; i++)
    {
      msg_client.deconnect = stop_client;
      memcpy(&(msg_client.machine), m, sizeof(machine_info_t));
      
      // write on fd m
      safe_write(sock, &msg_client, sizeof(message_client_t));
      
      // read on fd serv
      ret = safe_read(sock, &msg_server, sizeof(message_server_t));

      if (ret == -1 || msg_server.deconnect)
        {
          return;
        }
    }

  // free monitoring
  free(m);

  // display
  for (int i = 0; i < msg_server.serv.max_users; i++)
    {
      if (msg_server.serv.client[i].active)
        display(&(msg_server.serv.client[i].machine_info));
    }

  // Stoping client
  while (1)
    {
      stop_client = 1;
      msg_client.deconnect = stop_client;
  
      safe_write(sock, &msg_client, sizeof(message_client_t));
      ret = safe_read(sock, &msg_server, sizeof(message_server_t));

      if (ret == -1 || msg_server.deconnect)
        {
          break;
        }
    }
}

static void handle_loop(int sock)
{
  int refresh_counter = 0;
  machine_info_t *m = NULL;
  message_client_t msg_client;
  message_server_t msg_server;
  
  while (1)
    {
      refresh_counter++;

      // monitoring
      m = sensor();

      msg_client.deconnect = stop_client;
      memcpy(&(msg_client.machine), m, sizeof(machine_info_t));
      free(m);
      
      // write on fd m
      safe_write(sock, &msg_client, sizeof(message_client_t));
      
      // read on fd serv
      int ret = safe_read(sock, &msg_server, sizeof(message_server_t));

      if (ret == -1 || msg_server.deconnect)
        {
          break;
        }

      // display
      printf("\n");
      printf(BOLDRED "Refresh: %d\n" RESET, refresh_counter);
      
      for (int i = 0; i < msg_server.serv.max_users; i++)
        {
          if (msg_server.serv.client[i].active)
            display(&(msg_server.serv.client[i].machine_info));
        }
    }
}

static char *get_time(unsigned long long t)
{
  char *str_time = malloc(sizeof(char) * 32);
  str_time[31] = '\0';

  int sec = t % 60;
  int min = t / 60;
  int hou = 0;

  if (min >= 60)
    {
      hou = min / 60;
      min = min % 60;
    }

  sprintf(str_time, "%02d:%02d:%02d", hou, min, sec);
  
  return str_time;
}

void regular_print ( arg_t * a)
{
  // Resular Print

  attron(A_BOLD);               
  mvprintw( 0, 0, "Machine:");
  attroff(A_BOLD);
  printw(" %s\n", a->content[current_machine].machine_info.name);

  attron(A_BOLD);               
  mvprintw( 1, 0, "Processor:");
  attroff(A_BOLD);
  printw(" %d", a->content[current_machine].machine_info.nproc);

  attron(A_BOLD);
  mvprintw( 2, 0, "Memory:");
  attroff(A_BOLD);
  printw(" %ld pages\n", a->content[current_machine].machine_info.mem_size);

  attron(A_BOLD);
  mvprintw( 3, 0, "Process:");
  attroff(A_BOLD);
  printw(" %ld\n", a->content[current_machine].machine_info.nprocess);
  
  char *str_time = NULL;
  attron(A_BOLD);
  mvprintw( 4, 0, "%5s" " %10s"  " %10s"  " %5s" " %5s"   " %6s" " %10s" " %10s"  " %9s"  " %s\n",
            "TID", "USER", "GROUP", "PPID", "CPU", "CPU\%",  "RES", "VIRT", "TIME", "COMMAND");
  attroff(A_BOLD);

  for (int i = 0; &(a->content[current_machine].machine_info.proc_info[i]) != NULL && (i+5)  < (a->row - 1) && (a->deb+i)  < (a->content[current_machine].machine_info.nprocess)  && (a->deb + a->row - 7) < (a->content[current_machine].machine_info.nprocess); i++)
    {
      // Transform time
      str_time = get_time(a->content[current_machine].machine_info.proc_info[i+a->deb].utime);

      mvprintw( i+5, 0, "%5d %10s %10s %5d %5d %6.2lf %10ld %10ld %9s %s\n",
                a->content[current_machine].machine_info.proc_info[i+a->deb].tid,
                a->content[current_machine].machine_info.proc_info[i+a->deb].ruser,
                a->content[current_machine].machine_info.proc_info[i+a->deb].rgroup,
                a->content[current_machine].machine_info.proc_info[i+a->deb].ppid,
                a->content[current_machine].machine_info.proc_info[i+a->deb].processor,
                (double)a->content[current_machine].machine_info.proc_info[i+a->deb].pcpu / 100,
                a->content[current_machine].machine_info.proc_info[i+a->deb].rss,
                a->content[current_machine].machine_info.proc_info[i+a->deb].size,
                str_time,
                a->content[current_machine].machine_info.proc_info[i+a->deb].cmd);

      // Clean
      free(str_time);
    }       
}

static void search_client(arg_t *a)
{
  if (current_machine > MAX_CLIENT - 1)
    {
      current_machine = current_machine % MAX_CLIENT;
    }
  
  if (a->content[current_machine].active)
    return;

  for (int i = 0; i < MAX_CLIENT; i++)
    {
      int tmp = (current_machine + i) % MAX_CLIENT;
      if (a->content[tmp].active)
        {
          current_machine = tmp;
          return;
        }
    }

  exit(EXIT_FAILURE);
}

static void * n_display ( void * arg)  {
  int ch = 0;
  arg_t * a = (arg_t *) arg;
  // Compute info

  initscr();                                      /* Start curses mode            */
  raw();                                          /* Line buffering enabled       */
  //cbreak();                                     /* Line buffering disabled      */
  keypad(stdscr, TRUE);                           /* We get F1, F2 etc..          */
  noecho();                                       /* Don't echo() while we do getch */
  nodelay(stdscr, TRUE);                          /* Reading from std don't lock*/

  do {
    if( ch == ERR) {
      pthread_mutex_lock ( a->m);
                        
      regular_print( a);

      pthread_cond_wait(a->c, a->m);
      pthread_mutex_unlock ( a->m);
                        
    }
    else if(ch == KEY_RESIZE) {
      pthread_mutex_lock(a->m);
      // Resize Window
      struct winsize w;
      ioctl(0, TIOCGWINSZ, &w);
      a->row = w.ws_row;
      a->col = w.ws_col;
                        
      clear();
      refresh();
      regular_print ( a);

      pthread_mutex_unlock(a->m);
    }
    else if ( ch == KEY_UP || ch == 'k') {
      pthread_mutex_lock(a->m);
      if ( a->deb > 0)        {
        a->deb--;
      }
      regular_print( a);
      pthread_mutex_unlock(a->m);
    }
    else if ( ch == KEY_DOWN || ch == 'j') {
      pthread_mutex_lock(a->m);
      if ( (a->deb + a->row - 7) < a->content[current_machine].machine_info.nprocess)  {
        a->deb++;
      }
      regular_print( a);
      pthread_mutex_unlock(a->m);
    }
    else if (ch == KEY_RIGHT || ch == 'l') {
      pthread_mutex_lock(a->m);


        {
          current_machine = current_machine + 1;
        }
      search_client(a);
      regular_print(a);

      pthread_mutex_unlock(a->m);
    }
    else if(ch == 'q' || ch == 'Q') {
      stop_client = 1;
    }
    attron(A_BOLD);
    mvprintw ( a->row-1, 0, "q:");
    attroff(A_BOLD);
    mvprintw ( a->row-1, 2, " quit");
    refresh();

    ch = getch();
    
  } while ( !stop_client);
  endwin();
  return NULL;
}

static void handle_ncurses(int sock)
{
  int refresh_counter = 0;
  machine_info_t *m = NULL;
  message_client_t msg_client;
  message_server_t msg_server;
  arg_t *a = malloc(sizeof(arg_t));
  struct winsize w;
  ioctl(0, TIOCGWINSZ, &w);

  pthread_t displayer;
  pthread_mutex_t m_ncurses = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t c_ncurses = PTHREAD_COND_INITIALIZER;  
  
  a->deb = 0;
  a->row = w.ws_row;
  a->col = w.ws_col;
  a->m = &m_ncurses;
  a->c = &c_ncurses;  

  pthread_create ( &displayer, NULL, n_display, a);

    
  while (1)
    {
      refresh_counter++;

      // monitoring
      m = sensor();

      msg_client.deconnect = stop_client;
      memcpy(&(msg_client.machine), m, sizeof(machine_info_t));
      free(m);
      
      // write on fd m
      safe_write(sock, &msg_client, sizeof(message_client_t));
      
      // read on fd serv
      int ret = safe_read(sock, &msg_server, sizeof(message_server_t));

      if (ret == -1 || msg_server.deconnect)
        {
          pthread_mutex_lock(a->m);
          stop_client = 1;
          pthread_mutex_unlock(a->m);
                    
          pthread_join ( displayer, NULL);
          pthread_cond_destroy (&c_ncurses);
          pthread_mutex_destroy (&m_ncurses);
          free(a);
          break;
        }

      // display
      pthread_mutex_lock(a->m);

      memcpy ( a->content, msg_server.serv.client, sizeof(msg_server.serv.client));
      pthread_mutex_unlock( a->m);
      pthread_cond_signal ( a->c);

    }
}

static void handle_file(int sock, char *path)
{
  // Redirect printf
  int fd = open(path, O_CREAT | O_WRONLY, 0666);
  dup2(fd, STDOUT_FILENO);

  handle_standard(sock);

  close(fd);
}

static void handle_stop(int sig)
{
  stop_client = 1;
}

static void client_check_arg(int ipv, enum mode_client mode, char *path)
{
  // IP version
  if (ipv != 4 && ipv != 6)
    {
      fprintf(stderr, "Error: bad ip version\n");
      exit(EXIT_FAILURE);
    }

  // mode
  if (mode != OUTPUT_FILE && mode != STANDARD && mode != LOOP && mode != NCURSES)
    {
      fprintf(stderr, "Error: bad mode\n");
      exit(EXIT_FAILURE);
    }

  if (mode == OUTPUT_FILE && !path)
    {
      fprintf(stderr, "Error: bad mode\n");
      exit(EXIT_FAILURE);
    }
}

static int client_connect(const int ipv, const char *ip, const char *port)
{
  // Init address info variable
  struct addrinfo *addr_info  = NULL;
  struct addrinfo  hints;

  memset(&hints, 0, sizeof(hints));

  if (ipv == 4)
    {
      hints.ai_family = AF_INET;
    }
  else if (ipv == 6)
    {
      hints.ai_family = AF_INET6;
    }
  else
    {
      hints.ai_family = AF_INET;
    }

  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = AI_PASSIVE;
  
  // Call getaddinfo to get address info
  int ret = getaddrinfo(ip, port, &hints, &addr_info);

  if (ret < 0)
    {
      herror("getaddrinfo");
      exit(EXIT_FAILURE);
    }

  // Varaible to connect to soket and listen
  struct addrinfo *tmp_ai  = NULL;
  int sock = -1;
  int connected = 0;
  
  for (tmp_ai = addr_info; tmp_ai != NULL; tmp_ai = tmp_ai->ai_next)
    {
      sock = socket(tmp_ai->ai_family, tmp_ai->ai_socktype, tmp_ai->ai_protocol);

      if (sock < 0)
        {
          perror("socket");
          continue;
        }

      int ret = connect(sock, tmp_ai->ai_addr, tmp_ai->ai_addrlen);

      if (ret < 0)
        {
          perror("connect");
          continue;
        }

      connected = 1;
      break;
    }

  // Check if we are connected
  if (!connected)
    {
      fprintf(stderr, "Failed to connect on %s:%s\n", ip, port);
      exit(EXIT_FAILURE);
    }

  return sock;
}

void client(int ipv, enum mode_client mode, char *ip, char *port, char *path)
{
  // Checking argument
  client_check_arg(ipv, mode, path);

  // Connect
  int sock = client_connect(ipv, ip, port);

  // Select mode
  switch (mode)
    {
    case STANDARD:
      handle_standard(sock);
      break;
      
    case LOOP:
      //
      signal(SIGINT, handle_stop);
      
      handle_loop(sock);
      break;

    case NCURSES:
      handle_ncurses(sock);
      break;
      
    case OUTPUT_FILE:
      handle_file(sock, path);
      break;
      
    default:
      fprintf(stderr, "Error: bad mode\n");
      exit(EXIT_FAILURE);
      break;
    }

  // Clean
  shutdown(sock, SHUT_RDWR);

  close(sock);
}

