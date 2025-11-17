FUNC := g++
FLAGS := -O3 -lm -g -Werror -lzip

CPP_FILES := main.cpp gtfs.cpp raptor.cpp
OUT := main.exe

all: $(OUT)

$(OUT): $(CPP_FILES)
	$(FUNC) $(CPP_FILES) -o $(OUT) $(FLAGS)

clean:
	rm -f $(OUT)