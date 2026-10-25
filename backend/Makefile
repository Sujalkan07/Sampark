CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -Iinclude
LDFLAGS = -lsqlite3 -lssl -lcrypto -lws2_32 -lcrypt32

SRCS = src/Database.cpp \
       src/UserRepository.cpp \
       src/SessionRepository.cpp \
       src/Validator.cpp \
       src/AuthService.cpp \
       src/AuthController.cpp \
       src/HealthController.cpp \
       src/Server.cpp

SERVER_OBJS = $(SRCS:.cpp=.o) src/main.o
TEST_OBJS = $(SRCS:.cpp=.o) tests/backend_test.o

TARGET_SERVER = server.exe
TARGET_TEST = test_suite.exe

all: $(TARGET_SERVER) $(TARGET_TEST)

$(TARGET_SERVER): $(SERVER_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): $(TEST_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET_TEST)
	./$(TARGET_TEST)

clean:
	rm -f src/*.o tests/*.o $(TARGET_SERVER) $(TARGET_TEST) *.db *.db-wal *.db-shm

.PHONY: all test clean
