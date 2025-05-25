#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>


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

typedef enum {
  UP,
  DOWN,
  LEFT,
  RIGHT,
} Dir;

typedef struct {
  int x;
  int y;
} Point;

typedef struct {
  Point start;
  Point end;
  Dir direction;
} SnakeData;

void init_snake(SnakeData* snake) {
  snake->start = (Point){.x = 5, .y = 0};
  snake->end = (Point){.x = 0, .y = 0};
  snake->direction = RIGHT;
}

void init_snake_on_board(Board* b, SnakeData* s) {
  Point snake_body = s->end;
  while(snake_body.x != s->start.x || snake_body.y != s->start.y) {
    b->map[snake_body.y][snake_body.x] = Snake;  
    switch(s->direction) {
      case RIGHT:
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
    case RIGHT:
      b->map[s->start.y][(s->start.x)++] = Snake;
      break;
    case LEFT:
      b->map[s->start.y][(s->start.x)--] = Snake;
      break;
    case DOWN:
      b->map[(s->start.y)++][s->start.x] = Snake;
      break;
    case UP:
      b->map[(s->start.y)--][s->start.x] = Snake;
      break;
    default:
      printf("unsupported direction %c\n", s->direction);
  }

  // snake butt
 if(s->end.x < b->size_x && b->map[s->end.y][s->end.x+1] == Snake) {
    // check right
    b->map[s->end.y][s->end.x++] = Empty;
  } else if(s->end.x < b->size_x && b->map[s->end.y][s->end.x-1] == Snake) {
    // check left
    b->map[s->end.y][s->end.x--] = Empty;
  } else if(s->end.y+1 < b->size_y && b->map[s->end.y+1][s->end.x] == Snake) {
    // check down
    b->map[s->end.y++][s->end.x] = Empty;
  } else if(s->end.y < b->size_y && b->map[s->end.y-1][s->end.x] == Snake) {
    // check up
    b->map[s->end.y--][s->end.x] = Empty;
  }
}

void set_snake_direction(char c, SnakeData* s) {
  switch(c) {
    case 'd':
      s->direction = RIGHT;
      break;
    case 'a':
      s->direction = LEFT;
      break;
    case 'w':
      s->direction = UP;
      break;
    case 's':
      s->direction = DOWN;
      break;
    default:
      printf("unsuported input: %c\n", c);
  }
}

// ============================================================================

typedef struct {
  int valid;
  char c;
} OptChar;

typedef struct {
  char input;
  int buffer_full;
  pthread_mutex_t mutex;
  pthread_cond_t new_input;
  pthread_cond_t input_consumed;
} InputBuffer;

InputBuffer shared_buffer = {
  .input = ' ',
  .buffer_full = 0,
  .mutex = PTHREAD_MUTEX_INITIALIZER,
  .new_input = PTHREAD_COND_INITIALIZER,
  .input_consumed = PTHREAD_COND_INITIALIZER
};

char buffer_put() {
  pthread_mutex_lock(&shared_buffer.mutex);
  while(shared_buffer.buffer_full)
    pthread_cond_wait(&shared_buffer.input_consumed, &shared_buffer.mutex);
  char c = getchar();
  shared_buffer.buffer_full = 1;
  shared_buffer.input = c;
  pthread_cond_signal(&shared_buffer.new_input);
  pthread_mutex_unlock(&shared_buffer.mutex);
  return c;
}

OptChar buffer_get() {
  pthread_mutex_lock(&shared_buffer.mutex);
  if(!shared_buffer.buffer_full) {
    return (OptChar){.valid=0, .c='a'};
  }
  char c = shared_buffer.input;
  shared_buffer.buffer_full = 0;
  pthread_cond_signal(&shared_buffer.input_consumed);
  pthread_mutex_unlock(&shared_buffer.mutex);
  return (OptChar){.valid=1, .c=c};
}

void* input_loop() {
  for(;;) {
    char c = buffer_put();
    if(c == 'q') {
      break;
    }
  }
  return NULL;
}

void clear_screen() {
  printf("\033[H\033[J");
}
    
struct timespec ts = {
  .tv_sec = 0,
  .tv_nsec = 300000000L  // 300 ms
};

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

void main_loop() {
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  //int fps = 60;

  Board b;
  init_empty_board(&b, 20, 10);
  //b.map[0][0] = Snake;
  SnakeData snake;
  init_snake(&snake);
  init_snake_on_board(&b, &snake);

  for(;;) { 
    clear_screen();

    int ret = poll(fds, 1, 50);

    if (ret == -1) {
        perror("poll");
    } else if (ret == 0) {
        //printf("Czas minął!\n");
    } else if (fds[0].revents & POLLIN) {
      char c = getchar();
      if(c == 'w' || c == 's' || c == 'a' || c == 'd') set_snake_direction(c, &snake);
      if(c == 'q') {
        print_board(&b);
        break;
      }
    }

    update_snake(&b, &snake);

    print_board(&b);
    nanosleep(&ts, NULL);
  }

  free_board(&b);
}

void* main_loop_2() {
  Board b;
  init_empty_board(&b, 20, 10);
  //b.map[0][0] = Snake;
  SnakeData snake;
  init_snake(&snake);
  init_snake_on_board(&b, &snake);

  for(;;) {
    clear_screen();

    OptChar input = buffer_get();
    if(input.valid) {
      printf("%c\n", input.c);
      if(input.c == 'q') {
        break;
      }
    }

    update_snake(&b, &snake);

    print_board(&b);
    nanosleep(&ts, NULL);
  }
  free_board(&b);

  return NULL;
}

// ============================================================================

int main() {
  //set_input_mode(0);
  set_input_mode(1);
  //main_loop();

  pthread_t main_loop_thread, input_thread;
  int ml_id = 1, inp_id = 1;

  pthread_create(&input_thread, NULL, input_loop, &inp_id);
  pthread_create(&main_loop_thread, NULL, main_loop_2, &ml_id);

  pthread_join(main_loop_thread, NULL);
  pthread_join(input_thread, NULL);

  set_input_mode(0);

  return 0;
}
