#==============================================================================#
#                                    NAMES                                     #
#==============================================================================#

NAME		= webserv
#NAME_BONUS	= $(NAME)_bonus
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -Wshadow -std=c++98
INCLUDES	= -I./includes
INCLUDES	+= -I./includes/http
INCLUDES	+= -I./includes/epoll
INCLUDES	+= -I./includes/config

#==============================================================================#
#                                    PATHS                                     #
#==============================================================================#

SRC_DIR		= src
OBJ_DIR		= obj

VPATH =		$(SRC_DIR)
VPATH +=	$(SRC_DIR)/epoll
VPATH +=	$(SRC_DIR)/http
VPATH +=	$(SRC_DIR)/config
VPATH +=	$(SRC_DIR)/config/parser

#==============================================================================#
#                                   SOURCES                                    #
#==============================================================================#

GENERAL	=	main.cpp
GENERAL	+=	utils.cpp

EPOLL = EpollServer.cpp
#EPOLL += EpollClient.cpp

HTTP =	HttpRequest.cpp
HTTP +=	HttpParser.cpp
HTTP +=	HttpResponse.cpp

FILE_PARSE = ConfigParser.cpp
FILE_PARSE += ConfigParserLocation.cpp
FILE_PARSE += ConfigParserLocationDirectives.cpp
FILE_PARSE += ConfigParserServer.cpp
FILE_PARSE += ConfigParserServerDirectives.cpp

FILE_CONFIG	= Validator.cpp
FILE_CONFIG	+= ValidatorUtils.cpp
FILE_CONFIG += ValidatorUtils2.cpp
FILE_CONFIG	+= Tokenizer.cpp
FILE_CONFIG	+= UtilsConfig.cpp
FILE_CONFIG += ServerConfig.cpp

SRC	=	$(GENERAL)
SRC	+=	$(HTTP)
SRC	+=	$(EPOLL)
SRC	+=	$(FILE_CONFIG)
SRC +=	$(FILE_PARSE)

OBJ			= $(SRC:%.cpp=$(OBJ_DIR)/%.o)

#SRC_BONUS	= $(SRC)
#OBJ_BONUS	= $(SRC_BONUS:%.cpp=$(OBJ_DIR)/%.o)

#==============================================================================#
#                                    RULES                                     #
#==============================================================================#

all: $(NAME)

#mandatory
$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)
#	rm -f $(NAME_BONUS)

re: fclean all

#bonus: $(NAME_BONUS)

#$(NAME_BONUS): $(OBJ_BONUS)
#	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJ_BONUS) -o $(NAME_BONUS)

val: all
	valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./$(NAME)

.PHONY: all clean fclean re val #bonus