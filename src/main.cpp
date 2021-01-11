#include <iostream>
#include "Datfile.hpp"

int main()
{
	std::string fname;

	std::cout << "Enter file name to convert: " << std::endl;
	std::cin >> fname;

	if (Datfile::convert(fname))
	{
		std::cout << "yeehaw" << std::endl;
	}

	return 0;
}