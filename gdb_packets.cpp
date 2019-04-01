#include <iostream>

#include "gdb_packets.h"

using namespace std;

int fromhex(int a)
{
    if (a >= '0' && a <= '9')
        return a - '0';
    else if (a >= 'a' && a <= 'f')
        return a - 'a' + 10;
    else if (a >= 'A' && a <= 'F')
        return a - 'A' + 10;
    else
        cerr << "Invalid HEX digit: '" << (char)a << "'\n";
    return 0;
}

int tohex(int nib)
{
    if (nib < 10)
        return '0' + nib;
    else
        return 'a' + nib - 10;
}

size_t hex2bin(const string& hex, vector<uint8_t>& bin)
{
    size_t i;
    for (i = 0; i < hex.size()/2; i++) {
        if (hex[i*2] == 0 || hex[i*2+1] == 0) {
            /* Hex string is short, or of uneven length.
             Return the count that has been converted so far.  */
            return i;
        }
        bin.push_back(fromhex (hex[i*2+0]) * 16 + fromhex (hex[i*2+1]));
    }
    return i;
}

void bin2hex(const void *ptr, size_t size, string &str)
{
    auto bin = static_cast<const char*>(ptr);
    while (size --> 0) {
        auto ch = *bin++;
        char h1 = tohex((ch >> 4) & 0xf);
        char h2 = tohex(ch & 0xf);
        str.push_back(h1);
        str.push_back(h2);
    }
}
