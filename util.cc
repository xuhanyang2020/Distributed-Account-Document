#include <iostream>

#include "util.h"



map<string, nodeConfig> node2Configuration;
unordered_map<string,int> serverName2ID;
unordered_map<int, string> ID2ServerName;

void print(string str) {
    // printf_lock.lock();
    std::cout<<str<<std::endl;
    // printf_lock.unlock();
}
