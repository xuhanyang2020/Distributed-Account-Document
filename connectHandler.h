#include<fstream>
#include<iostream>
#include<string>
#include<sstream>
#include<map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#include "util.h"

using namespace std;

void getConfigure(string fileName, string identifer);

int setupListener(string port);

void* localSender(void* param);

void* localReceiver(void* param);

void* fakeTransaction(void* param);

void* transactionGenerator(void* param);