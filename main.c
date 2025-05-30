#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>

#define MAX_FOOD 1
#define MIN_REFRESH_TIME 1000000L
#define MAX_EXP_SIZE 1000

#define SNAKE_SYMBOL "O"
#define SNAKE_OPEN_MOUTH "O"
#define FOOD_SYMBOL "𝛅"
#define FlOOR_SYMBOL "."
#define BORDER_SYMBOL "▣"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

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
  Dir **dirMap;
  Point start;
  Point end;
  int tummy;
  int animation;
  Dir direction;
} SnakeData;

typedef enum {
  Empty,
  Snake,
  Food,
  Border,
} BoardField;

typedef struct {
  BoardField **map;
  int size_x;
  int size_y;
  int food;
} Board;

typedef struct {
  BoardField** old_state;
  BoardField** new_state;
  Dir move;
  int done;
} Exp;

typedef struct {
  Exp arr[MAX_EXP_SIZE];
  size_t id;
} ExpArray;

void add_experience(ExpArray* arr, Exp e) {
  arr->arr[arr->id++] = e;
}

void clear_screen() {
  printf("\033[H\033[J");
}
    
struct timespec ts = {
  .tv_sec = 0,
  .tv_nsec = 300000000L  // 300 ms
};
int updated_time = 1;

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

void exit_program(const char* message, ...) {
  va_list args;
  va_start(args, message);

  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), message, args);
  va_end(args);

  fprintf(stderr, "Error: %s\n", buffer);
  set_input_mode(0);
  exit(1);
}

float rand_float(float a, float b) {
  return a + (b - a) * ((float) rand() / RAND_MAX);
}

//-----------------------------------------------------------------------------

typedef struct {
  float** mat;
  int rows;
  int cols;
} Matrix;

Matrix alloc_matrix(int n, int m) {
  float** mat = malloc(n * sizeof(float*));
  for(int i = 0; i < n; i++) {
    mat[i] = malloc(m * sizeof(float));
  }
  return (Matrix){.mat=mat, .rows=n, .cols=m};
}

void free_matrix(Matrix* A) {
  for(int i = 0; i < A->rows; i++) {
    free(A->mat[i]); 
  }
  free(A->mat);
}

void rand_matrix(Matrix* A, float a, float b) {
  for(int i = 0; i < A->rows; i++) {
    for(int j = 0; j < A->cols; j++) {
      A->mat[i][j] = rand_float(a, b);
    }
  }
}

void matmul(Matrix* A, Matrix* B, Matrix* C) {
  if(A->cols != B->rows || C->rows != A->rows || C->cols != B->cols) {
    exit_program(
      "Tried to multiply matricies with sizes %dx%d, %dx%d to get matrix %dx%d",
      A->rows, A->cols, B->rows, B->cols, C->rows, C->cols);
  }

  for(int i = 0; i < C->rows; i++) {
    for(int j = 0; j < C->cols; j++) {
      for(int k = 0; k < A->cols; k++) {
        C->mat[i][j] += A->mat[i][k] * B->mat[k][j];
      }
    }
  }
}

void print_matrix(Matrix* A) {
  printf("%d x %d\n", A->rows, A->cols);
  for(int i = 0; i < A->rows; i++) {
    for(int j = 0; j < A->cols; j++) {
      printf("%f  ", A->mat[i][j]);
    }
    printf("\n");
  }
}

//-----------------------------------------------------------------------------

int lost_game = 0;

void generate_food(Board*b, int prob) {
  int make_food = rand() % 100;
  if(make_food > prob && b->food <= MAX_FOOD) {
    int new_x = rand() % b->size_x;
    int new_y = rand() % b->size_y;
    if(b->map[new_y][new_x] == Snake ||
       new_y == 0 || new_x == 0 ||
       new_y == b->size_y-1 || new_x == b->size_x-1) return;
    b->map[new_y][new_x] = Food;
    b->food++;
  }
}

void print_field(Board* b, SnakeData* s, int i, int j) {
  switch(b->map[i][j]) {
    case Empty:
      printf(FlOOR_SYMBOL);
      break;
    case Snake:
      if(!s->animation && i == s->start.y && j == s->start.x) {
        printf(SNAKE_OPEN_MOUTH);
      } else {
        printf(SNAKE_SYMBOL);
      }
      break;
    case Food:
      printf(FOOD_SYMBOL);
      break;
    case Border:
      printf(BORDER_SYMBOL);
      break;
  }
}

void print_board(Board* b, SnakeData* s) {
  for(int i = 0; i < b->size_y; i++) {
    for(int j = 0; j < b->size_x; j++) {
      print_field(b, s, i, j);
    }
    printf("\n");
  }
}

void init_empty_board(Board* b, int size_x, int size_y) {
  b->food = 0;
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
      if(i == 0 || j == 0 || j == b->size_x - 1 || i == b->size_y - 1) {
        b->map[i][j] = Border;
      }
      else {
        b->map[i][j] = Empty;
      }
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

void init_snake(SnakeData* snake, Board* b) {
  snake->start = (Point){.x = 3, .y = 2};
  snake->end = (Point){.x = 2, .y = 2};
  snake->direction = RIGHT;
  snake->tummy = 0;
  snake->animation = 0;

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
  s->animation = s->animation == 0 ? 1 : 0;
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
  if(b->map[s->start.y][s->start.x] == Snake ||
      b->map[s->start.y][s->start.x] == Border) {
    //exit_program("snake eating itself!");
    lost_game = 1;
    return;
  }
  int ate = 0;
  if(b->map[s->start.y][s->start.x] == Food) {
    ate = 1;
    updated_time = 0;
    s->tummy++;
    b->food--;
  }
  b->map[s->start.y][s->start.x] = Snake;

  // if we ate food we make our snake longer by not reducing tail this frame
  if(ate) return;
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
      if(s->direction != LEFT)
        s->direction = RIGHT;
      break;
    case 'a':
      if(s->direction != RIGHT)
        s->direction = LEFT;
      break;
    case 'w':
      if(s->direction != DOWN)
        s->direction = UP;
      break;
    case 's':
      if(s->direction != UP)
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

char dir_to_char(Dir m) {
  switch(m) {
    case LEFT:
      return 'a';
      break;
    case RIGHT:
      return 'd';
      break;
    case UP:
      return 'w';
      break;
    case DOWN:
      return 's';
      break;
    case NIL:
      return 'a';
      break;
  }
}

void reset_env();
Dir e_greedy() {
  int p = (rand() % 100) % 4;
  switch(p) {
    case 0:
      return LEFT;
    case 1:
      return RIGHT;
    case 2:
      return UP;
    case 3:
      return DOWN;
  }
  return DOWN;
}

void execute_move(SnakeData *s, Dir move) {
  set_snake_direction(dir_to_char(move), s); 
}

ExpArray replay_buffer;

void run_simulation(Board* b, SnakeData *s, int verbose) {
  while(!lost_game) {
    Dir move = e_greedy();

    execute_move(s, move);
    update_snake(s, b);
    generate_food(b, 50);

    if(verbose) {
      clear_screen();
      print_board(b, s);
      printf("Snake made move: %c\n", dir_to_char(move));
      nanosleep(&ts, NULL);
    }
  }

  lost_game = 0;
  return;
}

/* 
 * model
 * params
 */
void train() {
  /*
  FOR each episode:
    reset environment
    s = initial state

    WHILE not done:
        choose a from s using epsilon-greedy
        take action a → get s', r, done
        store (s, a, r, s', done)
        sample batch and train Q
        s ← s'

    decay epsilon
  END
  */
}

//------------------------------------------------------------------------------

void main_loop() {
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  Board b;
  init_empty_board(&b, 10, 10);
  b.map[4][4] = Food;
  SnakeData snake;
  init_snake(&snake, &b);

  for(;;) { 
    clear_screen();

    int ret = poll(fds, 1, 50);

    if (ret == -1) {
        perror("poll");
    } else if (ret == 0) {
    } else if (fds[0].revents & POLLIN) {
      char c = getchar();
      flush_stdin();
      if(c == 'w' || c == 's' || c == 'a' || c == 'd') set_snake_direction(c, &snake);
      if(c == 'q') {
        print_board(&b, &snake);
        //print_snake_directions(&snake, &b);
        break;
      }
    }

    update_snake(&snake, &b);
    if(snake.tummy % 4 == 0 && !updated_time) {
      ts.tv_nsec -= 10000000;
      ts.tv_nsec = max(ts.tv_nsec, MIN_REFRESH_TIME);
      updated_time = 1;
    }
    if(lost_game) {
      print_board(&b, &snake);
      printf("Lost game!\n");
      return;
    }
    generate_food(&b, 40);

    print_board(&b, &snake);
    printf("player score: %d\n", snake.tummy);
    printf("food on board: %d\n", b.food);
    printf("current refresh rate: %ld\n", ts.tv_nsec);
    //print_snake_directions(&snake, &b);
    nanosleep(&ts, NULL);
  }

  free_snake(&snake, b.size_y);
  free_board(&b);
}


// ============================================================================

int main() {
  srand(time(NULL));

  set_input_mode(1);

  /*
  Matrix A = alloc_matrix(2, 4);
  Matrix B = alloc_matrix(4, 2);
  Matrix C = alloc_matrix(2, 2);
  rand_matrix(&A, 0.0, 1.0);
  rand_matrix(&B, 0.0, 1.0);
  print_matrix(&A);
  print_matrix(&B);
  matmul(&A, &B, &C);
  print_matrix(&C);
  free_matrix(&A);
  free_matrix(&B);
  free_matrix(&C);
  */

  //main_loop();

  Board b;
  init_empty_board(&b, 10, 10);
  SnakeData snake;
  init_snake(&snake, &b);

  run_simulation(&b, &snake, 1);

  free_snake(&snake, b.size_y);
  free_board(&b);

  set_input_mode(0);
  return 0;
}
