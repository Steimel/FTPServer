# Tommy Steimel

CXX	= g++

NAME	= my_ftpd
OFILES	= $(NAME).o

.SUFFIXES: .o .cpp

 all:	$(NAME)

 $(NAME): $(OFILES)
	$(CXX) $(OFILES) -o $(NAME)

 $(NAME).o: $(NAME).cpp
	$(CXX) -c -o $(NAME).o $(NAME).cpp

 clean:
	-rm *.o $(NAME)