# Opzioni del compilatore.
CC := g++
CXXFLAGS := -std=c++20 -O2

# Directories del progetto.
SRC_DIR := src
BIN_DIR := bin
LIB_DIR := lib

# Nome del programma da generare.
TARGET := $(BIN_DIR)/dedalo_engine

# Indico al linker quali librerie dinamiche collegare.
LDLIBS := -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -lSDL2

# Aggiungo le altre librerie come directory di inclusione
CXXFLAGS += -I$(LIB_DIR)/vkbootstrap
CXXFLAGS += -I$(LIB_DIR)/vkinitializers
CXXFLAGS += -I$(LIB_DIR)/vma

# Definisco la variabile SRC come l'elenco di tutti i nomi dei file sorgenti C++ (*.cpp) presenti nella directory SRC_DIR.
SRC := $(wildcard $(SRC_DIR)/*.cpp)

# Aggiungo i file sorgenti delle librerie alla lista dei file sorgenti da compilare
SRC += $(wildcard $(LIB_DIR)/vkbootstrap/*.cpp)
SRC += $(wildcard $(LIB_DIR)/vkinitializers/*.cpp)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm $(TARGET)
