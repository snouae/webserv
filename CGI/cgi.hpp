#ifndef CGI_H
#define CGI_H

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <string>
#include "/Users/snouae/Desktop/server/request/request.hpp"
std::string run_cgi(char *file, char *path, request &r);

#endif