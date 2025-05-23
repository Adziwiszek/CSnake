#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>

void set_input_mode(int enable) {
  static struct termios oldt, newt;

  if (!enable) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  } else {
    tcgetattr(STDIN_FILENO, &oldt); 
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); 
  }
}

// ============================================================================

typedef enum {
  Empty,
  Snake,
} BoardField;

typedef struct {
  BoardField **map;
  int size_x;
  int size_y;
} Board;

void print_field(Board* b, int i, int j) {
  switch(b->map[i][j]) {
    case Empty:
      printf(".");
      break;

    case Snake:
      printf("O");
      break;
  }
}

void print_board(Board* b) {
  for(int i = 0; i < b->size_y; i++) {
    for(int j = 0; j < b->size_x; j++) {
      print_field(b, i, j);
    }
    printf("\n");
  }
}

void init_empty_board(Board* b, int size_x, int size_y) {
  b->size_y = size_y;
  b->size_x = size_x;

  b->map = malloc(size_y * sizeof(BoardField*));
  if (b->map == NULL) {
    fprintf(stderr, "Malloc error for rows\n");
    exit(1);
  }
  
  for(int i = 0; i < size_y; i++) {
    b->map[i] = malloc(size_x * sizeof(BoardField));
    if (b->map[i] == NULL) {
      fprintf(stderr, "Mallloc error for row: %d\n", i);
      for(int j = 0; j < i; j++) {
        free(b->map[j]);
      }
      free(b->map);
      exit(1);
    }
  }
  
  for(int i = 0; i < b->size_y; i++) {
    for(int j = 0; j < b->size_x; j++) {
      b->map[i][j] = Empty;
    }
  }
}

void free_board(Board* b) {
  if(b->map != NULL) {
    for(int i = 0; i < b->size_y; i++) {
      free(b->map[i]);
    }
    free(b->map);
  }
}

// ============================================================================

void clear_screen() {
    printf("\033[H\033[J");
}

void main_loop() {
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  int fps = 60;

  Board b;
  init_empty_board(&b, 20, 10);

  for(;;) { 
    clear_screen();

    print_board(&b);
    int ret = poll(fds, 1, 50);

    if (ret == -1) {
        perror("poll");
    } else if (ret == 0) {
        //printf("Czas minął!\n");
    } else if (fds[0].revents & POLLIN) {
      char c = getchar();
      printf("Nacisnąłeś: %c\n", c);
      if(c == 'q') break;
    }

    usleep(100000);
  }

  free_board(&b);
}

// ============================================================================

int main() {
  //Board b;
  /*init_empty_board(&b, 20, 10);
  print_board(&b);
  free_board(&b);*/
  set_input_mode(1);
  main_loop();

  /*pthread_t main_loop_thread, input_thread;
  int ml_id = 1, inp_id = 1;

  pthread_create(&input_thread, NULL, input_loop, &inp_id);
  pthread_create(&main_loop_thread, NULL, main_loop, &ml_id);

  pthread_join(main_loop_thread, NULL);
  pthread_join(input_thread, NULL);
  */

  set_input_mode(0);

  return 0;
}
