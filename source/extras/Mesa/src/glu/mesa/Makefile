# src/glu/mesa/Makefile

TOP = ../../..

include $(TOP)/configs/current

GLU_MAJOR = 1
GLU_MINOR = 1
GLU_TINY = 0$(MESA_MAJOR)0$(MESA_MINOR)0$(MESA_TINY)

C_SOURCES = \
	glu.c \
	mipmap.c \
	nurbs.c \
	nurbscrv.c \
	nurbssrf.c \
	nurbsutl.c \
	polytest.c \
	project.c \
	quadric.c \
	tess.c \
	tesselat.c

OBJECTS = $(C_SOURCES:.c=.o)

INCLUDES = -I. -I- -I$(TOP)/include

##### RULES #####

.c.o:
	$(CC) -c $(INCLUDES) $(CFLAGS) $< -o $@

##### TARGETS #####

default:
	@if [ "${CONFIG_NAME}" = "beos" ] ; then \
		echo "$(GLU_LIB_NAME) not build under BeOS, but integrated into ${GL_LIB_NAME}." ; \
		exit 0 ; \
	else \
		$(MAKE) "${LIB_DIR}/${GLU_LIB_NAME}" ; \
	fi

$(LIB_DIR):
	-mkdir $(LIB_DIR)

# Make the library:
$(LIB_DIR)/$(GLU_LIB_NAME): $(OBJECTS)
	@CC=$(CC) CXX=$(CXX) $(TOP)/bin/mklib -o $(GLU_LIB) -major $(GLU_MAJOR) \
		-minor $(GLU_MINOR) -patch $(GLU_TINY) \
		$(MKLIB_OPTIONS) -install $(LIB_DIR) \
		$(GLU_LIB_DEPS) $(OBJECTS)

clean:
	-rm -f *.o */*.o */*/*.o
	-rm -f *.lo */*.lo */*/*.lo
	-rm -f *.la */*.la */*/*.la
