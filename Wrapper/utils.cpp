#include <fstream>
#include <sstream>

std::string readFile(const std::string& path)
{
	std::ifstream file(path);
	if (!file)
		throw std::runtime_error("cannot open " + path);
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}