#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>

#define DISTINCT_LETTER_COUNT 53

// MATH
int min(int a, int b) {
   return a<b ? a : b;
}

// STRING
int str_len(char* str) {
  int i, len=0;
  for(i=0;str[i];i++) len++;
  return len;
}

bool is_valid_char(char c) {
  return (c>=65 && c<=90) ||
         (c>=97 && c<=122) ||
         c==39;
}

// FILE
long int calc_filesize(FILE* f) {
  FILE* ff = f;
  fseek(ff, 0L, SEEK_END);
  return ftell(ff);
}
FILE* file_read_mode(char* name) {
  FILE* f = fopen(name, "r");
  if(f == NULL) {
    printf("Cannot open: %s\n", name);
    exit(0);
  }

  return f;
}

//----------------------------------------
// DICTIONARY
typedef struct TrieNode { 
  char c;
  bool isEnd;
  struct TrieNode* next[DISTINCT_LETTER_COUNT];
} Node;

Node* root;

Node* create_node(char c) {
  Node* n = malloc(sizeof(Node));
  n->c = c;
  n->isEnd = false;
  return n;
}

int get_trie_index(char c) {
  if(65<=c && c<=90)
    return c-65;
  else if(c==39)
    return 52;
  else
    return c-90;
}

bool is_next(Node* parent, char c) {
  int index = get_trie_index(c);
  return parent->next[index] != NULL;
}

Node* append_char(Node* parent, char c) {
  int index = get_trie_index(c);
  parent->next[index] = create_node(c);
  return parent->next[index];
}

void update_dictionary(char* word, int len) {
  Node* cur = root;
  int i, index;

  for(i=0;i<len;i++) {
    if( is_next(cur, word[i]) ) {
      index = get_trie_index(word[i]);
      cur = cur->next[ index ];
    }
    else
      break;
  }
  if(i == len) {
    cur->isEnd = true;
    return;
  }

  while(i<len) {
    cur = append_char(cur, word[i]);
    i++;
  }

  cur->isEnd = true;
}

bool is_correct_word(char* word, int len) {
  int i, 
      match_prefix_len = 0;
 
  Node* cur = root; 
  for(i=0;i<len;i++) {
    if( is_next(cur, word[i]) ) {
      match_prefix_len++;
      cur = cur->next[ get_trie_index(word[i]) ];
    }
    else {
      break;
    }
  }

  int index = get_trie_index(word[len-1]);
  if(len == match_prefix_len && cur->isEnd)
    return true;
  else
    return false;

  return false;
}

void init_dictionary(char* filename) {
  root = malloc( sizeof(Node) );
  char buff[20];
  int i, len;
  
  FILE *fptr;
  fptr = file_read_mode(filename);
  for(i=0;i<20;i++) buff[i] = 0;
  while( fscanf(fptr, "%s", buff)==1 ) {
    // printf("%s %d\n", buff, len);
    len = str_len(buff);
    update_dictionary(buff, len);
    for(i=0;i<20;i++) buff[i] = 0;
  }

  fclose(fptr);
}
//----------------------------------------
// FILE SEGMENTATION
typedef struct FileSegment {
  FILE *fptr;
  long int segment_start;
  long int segment_end;
  int line_start;
  long int file_end;
} file_segment;

typedef struct FileContainer {
  file_segment** fs_list;
  int fs_count;
} file_container;

file_container* create_file_container(file_segment** fs_list, int fs_count) {
  file_container* fc = malloc( sizeof(file_container) );
  fc->fs_list = fs_list;
  fc->fs_count = fs_count;
  
  return fc;
}

file_segment* create_file_segment(FILE* fptr, int segment_start, long int segment_end) {
  file_segment* fs = malloc( sizeof(file_segment) );
  fs->fptr = fptr;
  fs->segment_start = segment_start;
  fs->segment_end = segment_end;
  fs->line_start = 0;

  return fs;
}


file_segment** frag_file(char* file_name, int fragment_count) {
  file_segment** fs_list = calloc(fragment_count, sizeof(file_segment));
  int i, j;

  FILE* f = file_read_mode(file_name);
  long int file_size = calc_filesize(f);
  int seg_size = file_size / fragment_count;
  int seg_start, 
      seg_end = -1,
      line_count = 1;
  
  char buff_c = 0;

  for(i=0;i<fragment_count;i++) {
    FILE* fptr = file_read_mode(file_name);

    seg_start = seg_end + 1;
    seg_end = min(seg_start + seg_size - 1, file_size);
    fs_list[i] = create_file_segment(fptr, seg_start, seg_end);
    fs_list[i]->line_start = line_count;
    fseek(fptr, seg_start, SEEK_SET);

    while(ftell(fptr) <= seg_end) {
      if( fscanf(fptr, "%c", &buff_c)==EOF )
        break;
      if(buff_c=='\n') line_count++;
    }
    if(i==fragment_count-1) seg_end = file_size;
    else {
      while(ftell(fptr) <= file_size) {
        if( fscanf(fptr, "%c", &buff_c)==EOF ) 
          break;
        if(is_valid_char(buff_c)) seg_end++;
        else break;
        
      }
    }
    fs_list[i]->segment_end = seg_end;
    fs_list[i]->file_end = file_size;
  }
  
  return fs_list;
}

//----------------------------------------
// SPELL CHECKER
void do_spell_check(char* word, int len, int line_number) {
  if(!is_correct_word(word, len))
      printf("Incorrect word at line %d: %s\n", line_number, word);
}

void* parse_words(void* arg) {
  file_segment* fseg = arg;
  int cur_line = fseg->line_start;
  char buff[20];
  char buff_c;
  int buff_i = 0;
  int i;
  bool first_word_flag = true;

  for(i=0;i<20;i++) buff[i] = 0;
  fseek(fseg->fptr, fseg->segment_start, SEEK_SET);

  while(ftell(fseg->fptr) <= fseg->segment_end) {
    if( fscanf(fseg->fptr, "%c", &buff_c)==EOF )
      break;

    if( is_valid_char(buff_c) ) {
      buff[buff_i] = buff_c;
      buff_i++;
    } else {

      if(is_valid_char(buff[0]))
        do_spell_check(buff, buff_i, cur_line);
      
      // Update current line number
      if(buff_c == '\n') cur_line++;
      
      // Reset buffer
      for(buff_i = buff_i-1; buff_i>=0; buff_i--) buff[buff_i] = 0;
      buff_i = 0;
    }

  }

  if(buff_i>0) 
    do_spell_check(buff, buff_i, cur_line);
  
}

void start_spell_checker(file_container* f_container) {

  pthread_t thread_id[f_container->fs_count];
  int i;
  
  for(i=0;i<f_container->fs_count;i++)
    pthread_create(&thread_id[i], NULL, parse_words, f_container->fs_list[i]);

  for(i=0;i<f_container->fs_count;i++)
    pthread_join(thread_id[i], NULL);

}

int main() {
  char file_dictionary[] = "dict.txt";
  init_dictionary(file_dictionary);

  char input_file[] = "input.txt";  
  long int size = calc_filesize(file_read_mode(input_file));
  int segment_count = min( (int) sqrt(size), 100);
  file_segment** fs_list = frag_file(input_file, segment_count);
  file_container* f_container = create_file_container(fs_list, segment_count);

  start_spell_checker(f_container);

  return 0;
}