#pragma once

#include <string>
#include <vector>

int fromhex (int a);
int tohex (int nib);
size_t hex2bin(const std::string &hex, std::vector<uint8_t> &bin);
void bin2hex(const void *ptr, size_t size, std::string& str);
