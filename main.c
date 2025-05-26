#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>


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

void flush_stdin() {
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  char tmp;
  while (read(STDIN_FILENO, &tmp, 1) == 1) { }
  fcntl(STDIN_FILENO, F_SETFL, flags);
}

void exit_program(char* message) {
  printf("Message: %s\n", message);
  set_input_mode(0);
  exit(1);
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
    exit_program("Malloc error");
  }
  
  for(int i = 0; i < size_y; i++) {
    b->map[i] = malloc(size_x * sizeof(BoardField));
    if (b->map[i] == NULL) {
      fprintf(stderr, "Mallloc error for row: %d\n", i);
      for(int j = 0; j < i; j++) {
        exit_program("Malloc error");
      }
      free(b->map);
      exit_program("Malloc error");
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
  NIL,
} Dir;

typedef struct {
  int x;
  int y;
} Point;

typedef struct {
  Point start;
  Point end;
  Dir direction;
  Dir **dirMap;
} SnakeData;

void init_snake(SnakeData* snake, Board* b) {
  snake->start = (Point){.x = 5, .y = 0};
  snake->end = (Point){.x = 0, .y = 0};
  snake->direction = RIGHT;

  snake->dirMap = malloc(b->size_y * sizeof(Dir*));
  if(snake->dirMap == NULL) {
    fprintf(stderr, "Malloc error for rows\n");
    exit_program("Malloc error");
  }
  
  for(int i = 0; i < b->size_y; i++) {
    snake->dirMap[i] = malloc(b->size_x * sizeof(Dir));
    if (b->map[i] == NULL) {
      fprintf(stderr, "Mallloc error for row: %d\n", i);
      for(int j = 0; j < i; j++) {
        free(snake->dirMap[j]);
      }
      free(snake->dirMap);
      exit_program("Malloc error");
    }
  }
  
  for(int i = 0; i < b->size_y; i++) {
    for(int j = 0; j < b->size_x; j++) {
      snake->dirMap[i][j] = NIL;
    }
  }

  Point snake_body = snake->end;
  while(snake_body.x != snake->start.x || snake_body.y != snake->start.y) {
    snake->dirMap[snake_body.y][snake_body.x] = snake->direction;  
    b->map[snake_body.y][snake_body.x] = Snake;  
    switch(snake->direction) {
      case RIGHT:
        snake_body.x++;
        break;
      default:
        printf("unsupported direction %c\n", snake->direction);
        //exit(1);
    }
  }

  b->map[snake_body.y][snake_body.x] = Snake;
  snake->dirMap[snake_body.y][snake_body.x] = snake->direction; 
}

void free_snake(SnakeData* s, int board_size_y) {
  if(s->dirMap != NULL) {
    for(int i = 0; i < board_size_y; i++) {
      free(s->dirMap[i]);
    }
    free(s->dirMap);
  }
}

void update_snake(SnakeData* s, Board* b) {
  // change the direction at the turn
  s->dirMap[s->start.y][s->start.x] = s->direction; 
  // snake head
  switch(s->direction) {
    case RIGHT:
      s->start.x++;
      break;
    case LEFT:
      s->start.x--;
      break;
    case DOWN:
      s->start.y++;
      break;
    case UP:
      s->start.y--;
      break;
    default:
      printf("unsupported direction %c\n", s->direction);
  }
  if(s->start.x >= b->size_x || s->start.x < 0 || 
     s->start.y >= b->size_y || s->start.y < 0) {
    // we went out of bounds
    exit_program("snake went out of bounds");
  }
  s->dirMap[s->start.y][s->start.x] = s->direction; 
  b->map[s->start.y][s->start.x] = Snake;

  // snake butt
  Point old_end = s->end;
  b->map[s->end.y][s->end.x] = Empty;
  switch(s->dirMap[s->end.y][s->end.x]) {
    case RIGHT:
      s->end.x++;
      break;
    case LEFT:
      s->end.x--;
      break;
    case DOWN:
      s->end.y++;
      break;
    case UP:
      s->end.y--;
      break;
    case NIL:
      break;
    default:
      printf("unsupported direction %c\n", s->direction);
  }
  s->dirMap[old_end.y][old_end.x] = NIL;
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

void print_snake_directions(SnakeData* s, Board* b) {
  for(int i = 0; i < b->size_y; i++) {
    for(int j = 0; j < b->size_x; j++) {
      char c;
      switch(s->dirMap[i][j]) {
        case RIGHT:
          c = 'R';
          break;
        case LEFT:
          c = 'L';
          break;
        case DOWN:
          c = 'D';
          break;
        case UP:
          c = 'U';
          break;
        case NIL:
          c = '_';
          break;
        default:
          printf("unsupported direction %c\n", s->direction);
      }
      printf("%c", c);
    }
    printf("\n");
  }
}

// ============================================================================


void main_loop() {
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  //int fps = 60;

  Board b;
  init_empty_board(&b, 20, 10);
  //b.map[0][0] = Snake;
  SnakeData snake;
  init_snake(&snake, &b);

  for(;;) { 
    clear_screen();

    int ret = poll(fds, 1, 50);

    if (ret == -1) {
        perror("poll");
    } else if (ret == 0) {
        //printf("Czas minął!\n");
    } else if (fds[0].revents & POLLIN) {
      char c = getchar();
      flush_stdin();
      if(c == 'w' || c == 's' || c == 'a' || c == 'd') set_snake_direction(c, &snake);
      if(c == 'q') {
        print_board(&b);
        //print_snake_directions(&snake, &b);
        break;
      }
    }

    update_snake(&snake, &b);

    print_board(&b);
    //print_snake_directions(&snake, &b);
    nanosleep(&ts, NULL);
  }

  free_snake(&snake, b.size_y);
  free_board(&b);
}


// ============================================================================

int main() {
  set_input_mode(1);
  main_loop();
  set_input_mode(0);
  return 0;
}
