#-----------------------------------------------------------------------------#
#                             SWE Bridge Makefile                             #
#-----------------------------------------------------------------------------#
# SWE Bridge will be compiled at the directory ./build with name swebridge.   #
# This script will look for all *.c files in all subdirectories and add all   #
# all directories as candidates for header files.                             #
#                                                                             #
# This Makefile is based on Job Vranish's post at                             #
# https://spin.atomicobject.com                                               #
#-----------------------------------------------------------------------------#

# my program name
TARGET_EXEC ?= driver
# where to build
BUILD_DIR ?= ./build
# from here
SRC_DIRS ?= .

SRCS := $(shell find $(SRC_DIRS) -name '*.c' -not -path "./git/*")
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS ?= $(INC_FLAGS) -MMD -MP

LDFLAGS := -lrt -lm

$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Done!"


# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source (not used)
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) $(TARGET_EXEC)

-include $(DEPS)

MKDIR_P ?= mkdir -p


  

