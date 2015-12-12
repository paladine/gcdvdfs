#include <stdlib.h>
#include <string.h>
#include "Tokenizer.h"

Tokenizer::Tokenizer(const char *str) {
  this->str = strdup(str);
  tokenize();
}

Tokenizer::~Tokenizer() { free(str); }

void Tokenizer::tokenize() {
  char *saveptr;
  char *token;
  token = strtok_r(str, "/", &saveptr);
  while (token) {
    tokens.push_back(token);
    token = strtok_r(nullptr, "/", &saveptr);
  }
}
