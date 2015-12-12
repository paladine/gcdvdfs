#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

#include <vector>

class Tokenizer {
public:
  Tokenizer(const char *str);
  ~Tokenizer();

  const std::vector<const char *> getTokens() { return tokens; }

private:
  void tokenize();

  char *str;
  std::vector<const char *> tokens;
};

#endif
