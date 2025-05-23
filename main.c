#include <stdio.h>
#include <stdlib.h>

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

  // Alokuj pamięć dla tablicy wskaźników (wiersze)
  b->map = malloc(size_y * sizeof(BoardField*));
  if (b->map == NULL) {
    fprintf(stderr, "Błąd alokacji pamięci dla wierszy\n");
    exit(1);
  }
  
  // Alokuj pamięć dla każdego wiersza osobno
  for(int i = 0; i < size_y; i++) {
    b->map[i] = malloc(size_x * sizeof(BoardField));
    if (b->map[i] == NULL) {
      fprintf(stderr, "Błąd alokacji pamięci dla wiersza %d\n", i);
      // Zwolnij już zaalokowane wiersze przed wyjściem
      for(int j = 0; j < i; j++) {
        free(b->map[j]);
      }
      free(b->map);
      exit(1);
    }
  }
  
  // Zainicjalizuj wszystkie pola jako puste
  for(int i = 0; i < b->size_y; i++) {
    for(int j = 0; j < b->size_x; j++) {
      b->map[i][j] = Empty;
    }
  }
}

void free_board(Board* b) {
  if(b->map != NULL) {
    free(b->map);
  }
}

int main() {
  Board b;
  init_empty_board(&b, 20, 10);
  print_board(&b);
  free_board(&b);
  return 0;
}
