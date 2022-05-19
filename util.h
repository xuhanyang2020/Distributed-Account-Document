#include<string>
#include<map>
#include<unordered_map>
#include<queue>
#include<vector>
#include<mutex>
#include<queue>
#include<sstream>

#define BACKLOG 12
#define MAX_FD_SIZE 10

using namespace std;

struct nodeConfig {
    string ID;
    string address;
    string port;
    bool connected;

    nodeConfig() {};
    nodeConfig(string id, string addr, string p, bool conn = false) : ID(id), address(addr), port(p), connected(conn) {};
};

struct message {
    int messageType; // 0: proposal, 1: reply, 2: decision, 3: failure broadcast
    int transactionType; // 0: Deposit, 1: Transfer
    int amount;
    int proposalServerID;
    int messServerID;
    int priority;
    uint64_t time;
    // unsigned long currentTime;
    char transferer[20];
    char receiver[20]; 
};

struct recvParam {
    string identifier;
    int listener;

    recvParam(string id, int fd) : identifier(id), listener(fd) {};
};

struct cmp_pq {
    bool operator () (const message &msg1, const message &msg2) {
        if (msg1.priority != msg2.priority) {
            return msg1.priority > msg2.priority;
        } else {
            return msg1.messServerID > msg2.messServerID;
        }
    }
};

void print(string str);