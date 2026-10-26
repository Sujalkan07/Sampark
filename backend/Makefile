CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -Iinclude

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows_NT)
ifeq ($(findstring Linux,$(UNAME_S)),Linux)
    LDFLAGS = -lsqlite3 -lssl -lcrypto -lpthread
    TARGET_SERVER = server
    TARGET_TEST = test_suite
else
    LDFLAGS = -lsqlite3 -lssl -lcrypto -lws2_32 -lcrypt32
    TARGET_SERVER = server.exe
    TARGET_TEST = test_suite.exe
endif

SRCS = src/Database.cpp \
       src/UserRepository.cpp \
       src/SessionRepository.cpp \
       src/ConversationRepository.cpp \
       src/MessageRepository.cpp \
       src/Validator.cpp \
       src/AuthService.cpp \
       src/AuthController.cpp \
       src/MessagingService.cpp \
       src/MessagingController.cpp \
       src/HealthController.cpp \
       src/Server.cpp

SERVER_OBJS = $(SRCS:.cpp=.o) src/main.o
TEST_OBJS = $(SRCS:.cpp=.o) tests/backend_test.o

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
