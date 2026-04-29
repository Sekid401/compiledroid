CXX      = clang++
CXXFLAGS = -std=c++17 -O2 -ffunction-sections -fdata-sections -Wall
LDFLAGS  = -s -Wl,--gc-sections -lpthread
TARGET   = compiledroid

SRCS = main.cpp manifest.cpp scanner.cpp compiler.cpp packager.cpp \
       errorengine.cpp logger.cpp state.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install:
	cp $(TARGET) $(PREFIX)/bin/$(TARGET)

.PHONY: all clean install
