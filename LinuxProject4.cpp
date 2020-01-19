#include <iostream>
#include <string>
#include <unistd.h>
#include <getopt.h>
#include <xpcsc.hpp>

using namespace std;

void init_reader()
{
}

void read()
{
}

void write()
{
}

int main(int argc, char *argv[])
{
	if (argc < 2 || argc > 3)
	{
		std::cout << "wrong number of options\n";
		return 1;
	}

	option opts[] =
	{ 
		{ "read", no_argument, 0, 'r' },
		{ "write", required_argument, 0, 'w' },
	};
	
	int optindex = 0;
	
	char opchar = getopt_long(argc, argv, "rw:", opts, &optindex);
	
	switch (opchar)
	{
	case 'r':
		std::cout << "reading\n";
		break;
	case 'w':
		int id = atoi(optarg);
		std::cout << "writing " << id << std::endl;
	}
	return 0;
}