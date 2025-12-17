CPP       = c++
FLAGS     = -Wall -Wextra -Werror -std=c++98

SRCS      = main.cpp Webserv.cpp Server.cpp CGI.cpp
OBJS      = $(SRCS:.cpp=.o)
NAME      = webserv

all: $(NAME)
# make clean
# clear

$(NAME): $(OBJS)
	$(CPP) $(FLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp Webserv.hpp Server.hpp CGI.hpp
	$(CPP) $(FLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all