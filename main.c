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

typedef struct {
  int x;
  int y;
} Point;

typedef struct {
  Point start;
  Point end;
  char direction;
} SnakeData;

void init_snake(SnakeData* snake) {
  snake->start = (Point){.x = 5, .y = 0};
  snake->end = (Point){.x = 0, .y = 0};
  snake->direction = 'e';
}

void init_snake_on_board(Board* b, SnakeData* s) {
  Point snake_body = s->end;
  while(snake_body.x != s->start.x || snake_body.y != s->start.y) {
    b->map[snake_body.y][snake_body.x] = Snake;  
    switch(s->direction) {
      case 'e':
        snake_body.x++;
        break;
      default:
        printf("unsupported direction %c\n", s->direction);
        //exit(1);
    }
  }
}

void update_snake(Board* b, SnakeData* s) {
  // snake head
  switch(s->direction) {
    case 'e':
      b->map[s->start.y][(s->start.x)++] = Snake;
      break;
    default:
      printf("unsupported direction %c\n", s->direction);
  }

  // snake butt
  // check right
  if(s->end.x < b->size_x && b->map[s->end.y][s->end.x+1] == Snake) {
    b->map[s->end.y][s->end.x++] = Empty;
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
  //b.map[0][0] = Snake;
  SnakeData snake;
  init_snake(&snake);
  init_snake_on_board(&b, &snake);

    print_board(&b);
  for(;;) { 
    clear_screen();

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

    update_snake(&b, &snake);

    print_board(&b);
    usleep(300000);
    
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
