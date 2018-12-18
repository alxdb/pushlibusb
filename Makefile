PROJECT_NAME = $(notdir $(PWD))

bin = $(PROJECT_NAME).b
lib = $(PROJECT_NAME).a

src = $(filter-out main.c,$(wildcard *.c))
obj = $(patsubst %.c,%.o,$(src))

LDLIBS += -lusb-1.0
CFLAGS += -g

all : $(lib) $(bin)

$(lib) : $(obj)
	$(AR) rcs $@ $^

$(bin) : main.c $(lib)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

clean :
	-rm $(obj)
	-rm $(lib)
	-rm $(bin)

.PHONY: clean
