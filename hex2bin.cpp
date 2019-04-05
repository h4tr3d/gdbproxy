#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>

#include "gdb_packets.h"

using namespace std;

int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    for (int i = 1; i < argc; ++i) {
        string_view arg = argv[i];
        vector<char> bin;
        hex2bin(arg, bin);

        cout << argv[i] << ": ";
        copy(bin.begin(), bin.end(), ostream_iterator<char>(cout, ""));
        cout << '\n';
    }

    return 0;
}