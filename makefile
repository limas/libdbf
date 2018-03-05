
.SECONDEXPANSION:

PROJ_PATH=project
PROJ=$(notdir $(wildcard $(PROJ_PATH)/*))

BIN_PATH=bin
BIN=$(foreach proj,$(PROJ),$(BIN_PATH)/$(proj))

OBJ_PATH=obj

TRG=go
COMMON_SRC=$(wildcard ./*.c)
COMMON_INC=$(wildcard ./*.h)

ifeq ($(DBG),y)
DBG_OPT=-fsanitize=address 
else
DBG_OPT=
endif

$(shell mkdir -p $(BIN_PATH))
$(shell mkdir -p $(OBJ_PATH))

.PHONY: all clean echo $(BIN)

all: $(PROJ)

$(PROJ): $(BIN_PATH)/$$@

$(BIN): $$(wildcard $(PROJ_PATH)/$$(notdir $$@)/*.c)
	gcc -g -Wall $(DBG_OPT) -o $@ $^ $(COMMON_SRC) -I./ -I$(PROJ_PATH)/$(notdir $@)/

clean:
	rm -f *.o $(BIN_PATH)/*
	rm -f *.o $(OBJ_PATH)/*

echo:
	@echo $(PROJ)
	@echo $(BIN)

