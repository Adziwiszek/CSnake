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
#define MAX_EXP_SIZE 100000

#define SNAKE_SYMBOL "O"
#define SNAKE_OPEN_MOUTH "O"
#define FOOD_SYMBOL "𝛅"
#define FlOOR_SYMBOL "."
#define BORDER_SYMBOL "▣"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct {
  float** mat;
  int rows;
  int cols;
} Matrix;
void free_matrix(Matrix* A);

typedef enum {
  UP = 0,
  DOWN = 1,
  LEFT = 2,
  RIGHT = 3,
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
  Matrix* old_state;
  Matrix* new_state;
  int reward;
  int done;
  Dir move;
} Exp;

typedef struct {
  Exp arr[MAX_EXP_SIZE];
  size_t id;
} ExpArray;

int loop_back = 0;
void add_experience(ExpArray* arr, Exp e) {
  if(loop_back) {
    free_matrix(arr->arr[arr->id + 1].old_state);
    free_matrix(arr->arr[arr->id + 1].new_state);
    free(arr->arr[arr->id + 1].old_state);
    free(arr->arr[arr->id + 1].new_state);
  }
  arr->arr[arr->id++] = e;
  if(arr->id + 5 >= MAX_EXP_SIZE) {
    arr->id = 0;
    loop_back = 1;
  }
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


Matrix* alloc_matrix(int n, int m) {
  Matrix* mat_struct = malloc(sizeof(Matrix));
  if (!mat_struct) return NULL;

  mat_struct->rows = n;
  mat_struct->cols = m;

  mat_struct->mat = malloc(n * sizeof(float*));
  if (!mat_struct->mat) {
      free(mat_struct);
      return NULL;
  }
  for(int i = 0; i < n; i++) {
    mat_struct->mat[i] = malloc(m * sizeof(float));
    if (!mat_struct->mat[i]) {
      for (int j = 0; j < i; j++) {
        free(mat_struct->mat[j]);
      }
      free(mat_struct->mat);
      free(mat_struct);
      return NULL;
    }
  }
  return mat_struct;
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

/* Element-wise adds elements from B to A
 */
void elem_add(Matrix* A, Matrix* B) {
  if(A->cols != B->cols || A->rows != B->rows ) {
    exit_program(
      "Tried to add matricies with sizes %dx%d, %dx%d to each other",
      A->rows, A->cols, B->rows, B->cols);
  }
  for(int i = 0; i < A->rows; i++) {
    for(int j = 0; j < A->cols; j++) {
      A->mat[i][j] += B->mat[i][j];
    }
  }
}

void copy_matrix(Matrix* A, Matrix* B) {
  if(A->cols != B->cols || A->rows != B->rows ) {
    exit_program(
      "Tried to copy matrix with size %dx%d to %dx%d",
      A->rows, A->cols, B->rows, B->cols);
  }
  for(int i = 0; i < A->rows; i++) {
    for(int j = 0; j < A->cols; j++) {
      B->mat[i][j] = A->mat[i][j];
    }
  }
}

void ReLU(Matrix* A) {
  for(int i = 0; i < A->rows; i++) {
    for(int j = 0; j < A->cols; j++) {
      A->mat[i][j] = A->mat[i][j] > 0 ? A->mat[i][j] : 0;
    }
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

void reset_board(Board* b) {
  b->food = 0;
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
  reset_board(b);  
}

void free_board(Board* b) {
  if(b->map != NULL) {
    for(int i = 0; i < b->size_y; i++) {
      free(b->map[i]);
    }
    free(b->map);
  }
}

Matrix* board_to_matrix(Board* b) {
  Matrix* mat = malloc(sizeof(Matrix));
  mat = alloc_matrix(b->size_y, b->size_x);
  for(int i = 0; i < mat->rows; i++) {
    for(int j = 0; j < mat->cols; j++) {
      switch(b->map[i][j]) {
        case Border:
          mat->mat[i][j] = -1.0;
          break;
        case Snake:
          mat->mat[i][j] = 1.0;
          break;
        case Empty:
          mat->mat[i][j] = 0.0;
          break;
        case Food:
          mat->mat[i][j] = 1.0;
          break;
      }
    }
  }
  return mat;
}

// ============================================================================

void reset_snake(SnakeData* snake, Board* b) {
  snake->start = (Point){.x = 3, .y = 2};
  snake->end = (Point){.x = 2, .y = 2};
  snake->direction = RIGHT;
  snake->tummy = 0;
  snake->animation = 0;

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

void init_snake(SnakeData* snake, Board* b) {

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
 
  reset_snake(snake, b);
}

void free_snake(SnakeData* s, int board_size_y) {
  if(s->dirMap != NULL) {
    for(int i = 0; i < board_size_y; i++) {
      free(s->dirMap[i]);
    }
    free(s->dirMap);
  }
}

int update_snake(SnakeData* s, Board* b) {
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
    return -500;
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
  if(ate) return 100;
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
  return -1;
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
      printf("unsupported input: %c\n", c);
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
  return 'a';
}

void reset_env(Board* b, SnakeData* s) {
  reset_board(b);
  reset_snake(s, b);
}

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

typedef struct {
  Matrix* W_1; 
  Matrix* b_1;
  Matrix* W_2;
  Matrix* b_2;
  int hidden;
} Model;

void init_model(Model* m, int hidden) {
  m->hidden = hidden;
  m->W_1 = alloc_matrix(hidden, 100);
  m->b_1 = alloc_matrix(hidden, 1);
  m->W_2 = alloc_matrix(4, hidden);
  m->b_2 = alloc_matrix(4, 1);

  rand_matrix(m->W_1, 0.0, 1.0);
  rand_matrix(m->b_1, 0.0, 1.0);
  rand_matrix(m->W_2, 0.0, 1.0);
  rand_matrix(m->b_2, 0.0, 1.0);
}

void free_model(Model* m) {
  free_matrix(m->W_1);
  free_matrix(m->b_1);
  free_matrix(m->W_2);
  free_matrix(m->b_2);
}

Matrix* flatten(Matrix* A) {
  Matrix* x = alloc_matrix(A->rows * A->cols, 1);
  for(int i = 0; i < A->rows; i++) {
    for(int j = 0; j < A->cols; j++) {
      x->mat[i * A->cols + j][0] = A->mat[i][j];
    }
  }
  return x;
}

Matrix* forward(Model* m, Matrix* X) {
  Matrix* X_flat = flatten(X);
  Matrix* x_1 = alloc_matrix(m->hidden, 1);
  Matrix* out = alloc_matrix(4, 1);

  matmul(m->W_1, X_flat, x_1);
  elem_add(x_1, m->b_1);
  ReLU(x_1);

  matmul(m->W_2, x_1, out);
  elem_add(out, m->b_2);

  free_matrix(X_flat);
  free_matrix(x_1);
  free(X_flat);
  free(x_1);
  return out;
}

Dir get_best_move(Matrix* out, float eps) {
  Dir moves[] = {UP, DOWN, LEFT, RIGHT};
  float exploration = rand_float(0.0, 1.0);
  
  if(exploration > eps) {
    return moves[rand() % 4];
  } else {
    float max_val = out->mat[0][0];
    int max_id = 0;
    for(int i = 1; i < 4; i++) {
      if(out->mat[i][0] > max_val) {
        max_id = i;
        max_val = out->mat[i][0];
      }
    }
    return moves[max_id];
  }
}

float max_reward(Matrix* Q) {
  float max_r = 0.0;
  for(int i = 0; i < Q->rows; i++) {
    if(Q->mat[i][0] > max_r) {
      max_r = Q->mat[i][0];
    }
  }
  return max_r;
}

void backward(Model* m, ExpArray* rep_buffer) {
  float gamma = 0.3;
  float lr = 0.1;
  // get random memory and calculate loss
  Exp* batch = &rep_buffer->arr[rand() % rep_buffer->id];
  //Matrix* Q_pred = forward(m, batch->old_state);
  Matrix* X_flat = flatten(batch->old_state);
  Matrix* x_1 = alloc_matrix(m->hidden, 1);
  Matrix* x_1_act = alloc_matrix(m->hidden, 1);

  matmul(m->W_1, X_flat, x_1);
  elem_add(x_1, m->b_1);
  copy_matrix(x_1, x_1_act);
  ReLU(x_1_act);

  Matrix* Q_pred = alloc_matrix(4, 1);
  matmul(m->W_2, x_1, Q_pred);
  elem_add(Q_pred, m->b_2);

  float Q_sa = Q_pred->mat[batch->move][0];
  float target = 0.0;

  if(batch->done) {
    target = batch->reward;
  } else {
    Matrix* Q_new = forward(m, batch->new_state);
    float max_Q_new = max_reward(Q_new); 
    target = batch->reward + gamma * max_Q_new;
    free_matrix(Q_new);
    free(Q_new);
  }

  //float loss = (Q_sa - target) * (Q_sa - target);
  // do gradient descent
  int a = batch->move;
  float l_d = 2 * (Q_sa - target);

  // W_2, b_2
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < m->hidden; j++) {
      float grad = (i == a ? 1.0f : 0.0f) * l_d * x_1_act->mat[j][0];
      m->W_2->mat[i][j] -= lr * grad;
    }
    float grad_b = (i == a ? 1.0f : 0.0f) * l_d;
    m->b_2->mat[i][0] -= lr * grad_b;
  }
  
  // propagate through ReLU
  Matrix* delta1 = alloc_matrix(m->hidden, 1); // dL/dx1

  for(int j = 0; j < m->hidden; j++) {
    float grad = m->W_2->mat[a][j] * l_d;
    // pochodna ReLU
    if(x_1->mat[j][0] <= 0.0f) grad = 0.0f;
    delta1->mat[j][0] = grad;
  }

  // W_1, b_1
  for(int i = 0; i < m->hidden; i++) {
    for(int j = 0; j < X_flat->rows; j++) {
      float grad = delta1->mat[i][0] * X_flat->mat[j][0];
      m->W_1->mat[i][j] -= lr * grad;
    }
    m->b_1->mat[i][0] -= lr * delta1->mat[i][0];
  }
   
  free_matrix(delta1);
  free_matrix(Q_pred);
  free_matrix(X_flat);
  free_matrix(x_1);
  free_matrix(x_1_act);
  free(delta1);
  free(Q_pred);
  free(X_flat);
  free(x_1);
  free(x_1_act);
}

Model* model;
float exploration = 0.5;
int batch_size = 7;

void run_simulation(Board* b, SnakeData *s, int verbose, int iter) {
  while(!lost_game) {
    Matrix *s_before = board_to_matrix(b);
    Matrix *out = alloc_matrix(4, 1);

    out = forward(model, s_before);
    Dir move = get_best_move(out, exploration);
    exploration *= 0.9999;

    execute_move(s, move);
    int reward = update_snake(s, b);
    int done = lost_game;
    generate_food(b, 10);

    Matrix* s_after = board_to_matrix(b);

    Exp new_experience = {.old_state=s_before, .new_state=s_after, 
                          .done=done, .reward=reward, .move=move};

    add_experience(&replay_buffer, new_experience);

    int batch = replay_buffer.id < batch_size ? replay_buffer.id : batch_size;
    for(int i = 0; i < batch; i++) {
      backward(model, &replay_buffer);
    }

    if(verbose) {
      clear_screen();
      printf("iteration %d\n", iter);
      print_board(b, s);
      printf("Snake made move: %c\n", dir_to_char(move));
      nanosleep(&ts, NULL);
    }
    free_matrix(out);
  }
  lost_game = 0;
  return;
}

/* 
 * model 
 * params
 */
void train(int n_iters, int verbose) {
  Board b;
  init_empty_board(&b, 10, 10);
  SnakeData snake;
  init_snake(&snake, &b);

  replay_buffer.id = 0;

  model = malloc(sizeof(Model));
  init_model(model, 48);

  int log_every = (int)(n_iters / 10);

  for(int iter = 1; iter <= n_iters; iter++) {
    reset_env(&b, &snake);
    run_simulation(&b, &snake, verbose, iter);
  }

  free_model(model);
  free(model);
  free_snake(&snake, b.size_y);
  free_board(&b);
}

int main() {
  srand(time(NULL));

  set_input_mode(1);

  /*Matrix *S = alloc_matrix(64, 1);
  rand_matrix(S, 0.0, 1.0);
  Model* model = malloc(sizeof(Model));
  init_model(model, 48);

  Matrix *out = alloc_matrix(4, 1);
  out = forward(model, S);

  free_model(model);
  free_matrix(S);
  free(model);*/

  //main_loop();

  train(2000, 0);
  exploration = 0.0;
  train(5, 1);

  set_input_mode(0);
  return 0;
}
