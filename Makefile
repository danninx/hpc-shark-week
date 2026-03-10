CC = gcc
LEXER_FILES = build/roc_lexer.c
PARSER_FILES = build/roc_parser_integrated.tab.c build/roc_parser_integrated.tab.h
SERIAL_COMPILER = out/compiler
GENERATOR_BIN = out/generator
GENERATOR_SRC = roc_gen.c
HALE_BIN = out/hale_matcher
HALE_SRC = roc_match.c roc_match_test.c

all: $(LEXER_FILES) $(GENERATOR_BIN)

compiler: $(SERIAL_COMPILER)

generator: $(GENERATOR_BIN)

hale_matcher: $(HALE_BIN)

$(HALE_BIN): out_dir
	gcc -o $(HALE_BIN) $(HALE_SRC)

$(LEXER_FILES): $(PARSER_FILES) build_dir
	flex -o build/roc_lexer.c roc_values.l 

$(PARSER_FILES): build_dir
	bison -d roc_parser_integrated.y -o build/roc_parser.c --header=build/roc_parser.h

$(SERIAL_COMPILER): $(LEXER_FILES) $(PARSER_FILES)
	gcc -o $(SERIAL_COMPILER) $(LEXER_FILES) $(PARSER_FILES)

$(GENERATOR_BIN): out_dir
	$(CC) -o $(GENERATOR_BIN) $(GENERATOR_SRC)

build_dir:
	mkdir -p build

out_dir:
	mkdir -p out

clean:
	rm -f $(LEXER_FILES) $(PARSER_FILES) $(GENERATOR_BIN)

