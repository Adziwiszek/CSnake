#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>

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

char buffer_get() {
  pthread_mutex_lock(&shared_buffer.mutex);

  while(!shared_buffer.buffer_full)
    pthread_cond_wait(&shared_buffer.new_input, &shared_buffer.mutex);
   
  char c = shared_buffer.input;
  shared_buffer.buffer_full = 0;

  pthread_cond_signal(&shared_buffer.input_consumed);

  pthread_mutex_unlock(&shared_buffer.mutex);

  return c;
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

void* main_loop() {
  for(;;) {
    char input = buffer_get();
    printf("%c\n", input);
    if(input == 'q') {
      break;
    }
  }
  return NULL;
}

// ============================================================================

int main() {
  //Board b;
  /*init_empty_board(&b, 20, 10);
  print_board(&b);
  free_board(&b);*/
  set_input_mode(1);

  pthread_t main_loop_thread, input_thread;
  int ml_id = 1, inp_id = 1;

  pthread_create(&input_thread, NULL, input_loop, &inp_id);
  pthread_create(&main_loop_thread, NULL, main_loop, &ml_id);

  pthread_join(main_loop_thread, NULL);
  pthread_join(input_thread, NULL);

  set_input_mode(0);

  return 0;
}
