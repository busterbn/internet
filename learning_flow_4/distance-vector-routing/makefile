LINK_TARGET = main

SRCDIR = src
OBJS = $(SRCDIR)/main.o

REBUILDABLES = $(OBJS) $(LINK_TARGET)

all : $(LINK_TARGET)

clean:
	rm -f $(REBUILDABLES)

# Here is a Rule that uses some built-in Make Macros in its command:
# $@ expands to the rule's target, in this case "main".
# $^ expands to the rule's dependencies, in this case the object files
$(LINK_TARGET) : $(OBJS)
	cc -g -o $@ $^ -lpthread

# Here is a Pattern Rule, often used for compile-line.
# It says how to create a file with a .o suffix, given a file with a .c suffix.
# The rule's command uses some built-in Make Macros:
# $@ for the pattern-matched target
# $< for the pattern-matched dependency

$(SRCDIR)/%.o : $(SRCDIR)/%.c
	cc -g -Wall -o $@ -c $<

# These are Dependency Rules, which are rules without any command.
# Dependency Rules indicate that if any file to the right of the colon changes,
# the target to the left of the colon should be considered out-of-date.
# The commands for making an out-of-date target up-to-date may be found elsewhere
# (in this case, by the Pattern Rule above).
# Dependency Rules are often used to capture header file dependencies.

# wgetX.o : wgetX.h url.h
# url.o : url.h
