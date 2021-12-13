# I got tests 0-2 working with 0 errors, although once every like 40 tries I get a minor error (e.g. ERROR: Requested 115 bytes, read 114). Fuse wrapper tests work fine based on my quick demo. If you get different results than me you can let me know.

CFLAGS = -c -g -ansi -pedantic -Wall -std=gnu99 `pkg-config fuse --cflags --libs`

LDFLAGS = `pkg-config fuse --cflags --libs`

# Uncomment on of the following three lines to compile
# SOURCES= disk_emu.c sfs_api.c sfs_test0.c sfs_api.h
# SOURCES= disk_emu.c sfs_api.c sfs_test1.c sfs_api.h
SOURCES= disk_emu.c sfs_api.c sfs_test2.c sfs_api.h
# SOURCES= disk_emu.c sfs_api.c fuse_wrap_old.c sfs_api.h
# SOURCES= disk_emu.c sfs_api.c fuse_wrap_new.c sfs_api.h

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=jefftang_sfs

all: $(SOURCES) $(HEADERS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	gcc $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	gcc $(CFLAGS) $< -o $@

clean:
	rm -rf *.o *~ $(EXECUTABLE)
