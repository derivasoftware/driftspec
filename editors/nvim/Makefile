# Build the grammar of this repository into the plugin's runtimepath.
GRAMMAR := ../../grammar

all: parser/driftspec.so queries/driftspec/highlights.scm

parser/driftspec.so: $(GRAMMAR)/src/parser.c $(GRAMMAR)/src/scanner.c
	mkdir -p parser
	cc -shared -fPIC -O2 -I$(GRAMMAR)/src $(GRAMMAR)/src/parser.c $(GRAMMAR)/src/scanner.c -o $@

queries/driftspec/highlights.scm: $(GRAMMAR)/queries/highlights.scm $(GRAMMAR)/queries/folds.scm
	mkdir -p queries/driftspec
	cp $(GRAMMAR)/queries/*.scm queries/driftspec/

clean:
	rm -rf parser queries/driftspec

.PHONY: all clean
