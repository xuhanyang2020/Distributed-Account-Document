#include<algorithm>
#include<cstdio>
#include<iostream>
#include<string>
#include<sstream>
#include<vector>
#include<pthread.h>
#include<map>

#include "connectHandler.h"

using namespace std;

extern map<string, nodeConfig> node2Configuration;

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: ./mp1_node <identifier> <configuration file>\n");
        return -1;
    }

    string identifier = argv[1];
    string configFile = argv[2];

    getConfigure(configFile, identifier);

    // for (auto it = node2Configuration.begin(); it != node2Configuration.end(); it++) {
    //     printf("%s, %s, %s \n", node2Configuration[it->first].ID.c_str(), node2Configuration[it->first].address.c_str(), node2Configuration[it->first].port.c_str());
    // }

    // set up a listener
    int listener = setupListener(node2Configuration[identifier].port);
    
    if (listener == -1) {
        return -1;
    }

    pthread_t thread_localSend, thread_readGentx, thread_Recv;
    recvParam param(identifier, listener);

    pthread_create(&thread_localSend, NULL, localSender, &identifier);
    // pthread_create(&thread_readGentx, NULL, fakeTransaction, &identifier);
    pthread_create(&thread_readGentx, NULL, transactionGenerator, NULL);
    pthread_create(&thread_Recv, NULL, localReceiver, &param);

    pthread_join(thread_localSend, NULL);
    pthread_join(thread_Recv, NULL);
    pthread_join(thread_readGentx, NULL);

}
