#include <chrono>
#include <stdio.h>
#include <string.h>
#include <math.h> 

#include "connectHandler.h"

extern map<string, nodeConfig> node2Configuration;
extern unordered_map<string,int> serverName2ID;
extern unordered_map<int, string> ID2ServerName;

int thisID;
unordered_map<int, int> fd2ID; // sending socket and target serverid
unordered_map<int, int> ID2fd;
unordered_map<int, int> sockfdVec;   //id of server(0, 1, 2 ..... 7) to the connection socket    sending socket
int connectedServer = 0;
vector<int> disconnectHelper;

queue<message> sendBuffer;
mutex sendBuffer_lock;
priority_queue<message, vector<message>, cmp_pq> deliverBuffer; // all things waiting for deliver
unordered_map<uint64_t, unordered_map<int, pair<int, int> > > deliverHash; // time proposalServerID priority messServerID   all messages delivered already
mutex deliverBuffer_lock;

unordered_map<uint64_t, pair<int, message> > inProcessMsg;  // proposal server maintain the message generated in local server
mutex inProcessMsg_lock;

mutex printf_lock;
mutex waitSetup_lock;

vector<message> lastMsgs;

int timestamps;  // like lamport timestamp, to record priority
map<string, int> accounts;  // amounts of each account
mutex accounts_lock;

ofstream logFile;

void getConfigure(string fileName, string identifer) {
    ifstream iFile(fileName);
    string line;
    getline(iFile, line);
    
    while (getline(iFile, line)){
        stringstream sstr;
        sstr << line;

        string nodeId, address, portId;
        getline(sstr, nodeId, ' ');
        getline(sstr, address, ' ');
        getline(sstr, portId, ' ');
        
        node2Configuration[nodeId] = nodeConfig(nodeId, address, portId);
    }

    int ID = 0;
    // node2Configuration[nodeId] = nodeConfig(nodeId, address, portId);
    for (map<string, nodeConfig>::iterator it = node2Configuration.begin(); it != node2Configuration.end(); it++) {
        serverName2ID[it->first] = ID;
        ID2ServerName[ID] = it->first;
        ID++;
    } 

    thisID = serverName2ID[identifer];
    lastMsgs.resize(serverName2ID.size());
    // what is the definiton of connected server use bit manipulation
    connectedServer = pow(2, serverName2ID.size()) - 1; // assume all connected
    connectedServer -= (1 << thisID);   // remove local host

    for (int i = 0; i < lastMsgs.size(); i++) {
        lastMsgs[i].messageType = -1;
    }

    disconnectHelper.resize(serverName2ID.size(), 0);

    logFile.open("log_" + to_string(thisID) + ".txt", ios_base::out);
}

static void printOut(string str) {
    printf_lock.lock();
    cout<<str<<endl;
    printf_lock.unlock();
}

void printDB(string str = "0=>") {
    priority_queue<message, vector<message>, cmp_pq> tmp;
    message msg;
    
    logFile<<str<<" ";
    // printf("%s, ", str.c_str());
    while (deliverBuffer.size() > 0) {
        msg = deliverBuffer.top();
        deliverBuffer.pop();
        tmp.push(msg);

        string line = "[" + to_string(msg.time) + " " + to_string(msg.proposalServerID) + " " +
                      to_string(msg.messageType) + " " + to_string(msg.priority) + " " + to_string(msg.messServerID) + "]\t";
        // printf("[%ju, %s, %i, %i, %i]\t", msg.time,
        //         ID2ServerName[msg.proposalServerID].c_str(), msg.messageType, msg.priority, msg.messServerID);

        logFile<<line;
    }

    deliverBuffer = tmp;
    logFile<<endl;
    // printf("\n");
}

void printMsg(string str, message msg) {
    // string line = str + ": " + to_string(msg.time) + " " + ID2ServerName[msg.proposalServerID] + 
    //               to_string(msg.messageType) + " " + to_string(msg.priority) + " " + to_string(msg.messServerID) + "\n";
    // // printf("%s: %ju, %s, %i, %i, %i\n", str.c_str(), msg.time,
    // //         ID2ServerName[msg.proposalServerID].c_str(), msg.messageType, msg.priority, msg.messServerID);

    // logFile << line;

    // printDB();
}

// function for testing
void* fakeTransaction(void* param) {
    string identifier = *(string*) param;
    for (int i = 0; i < 10; i++) {
        message msg;

        msg.messageType = 0;
        msg.transactionType = 0;
        msg.amount = (i * 10 + 10 + thisID);
        msg.proposalServerID = thisID;

        memset(msg.transferer, '\0', 20 * sizeof(char));
        memcpy(msg.transferer, (identifier+"_"+to_string(i)).c_str(), sizeof(char) * 20);

        sendBuffer_lock.lock();
        sendBuffer.push(msg);
        sendBuffer_lock.unlock();
    }
}

// read transactions from local command line
void* transactionGenerator(void* param) {
    waitSetup_lock.lock();
    string line;
    while (true) {
        while (getline(cin, line)) {
            stringstream istr;
            istr<<line;

            string str;
            getline(istr, str, ' ');
            
            message msg;
            memset(msg.transferer, '\0' ,sizeof(char) * 20);
            memset(msg.receiver, '\0' ,sizeof(char) * 20);

            if (str == "DEPOSIT") {
                getline(istr, str, ' ');
                memcpy(&msg.transferer, str.c_str(), sizeof(char) * 20);
                getline(istr, str, ' ');
                msg.amount = atoi(str.c_str());
                msg.transactionType = 0;
            } else if (str == "TRANSFER") {
                string transferer;

                getline(istr, str, ' ');
                transferer = str;
                memcpy(&msg.transferer, str.c_str(), sizeof(char) * 20);
                getline(istr, str, ' ');
                getline(istr, str, ' ');
                memcpy(&msg.receiver, str.c_str(), sizeof(char) * 20);
                getline(istr, str, ' ');
                msg.amount = atoi(str.c_str());
                msg.transactionType = 1;

                // bool ctn = false;

                // accounts_lock.lock();
                // if (accounts.find(str) == accounts.end() || accounts[str] < msg.amount) {
                //     ctn = true;
                // }
                // accounts_lock.unlock();

                // if (ctn) {
                //     continue;
                // }
            } else {
                continue;
            }
            
            msg.time = chrono::duration_cast<chrono::nanoseconds>(
                        chrono::system_clock::now().time_since_epoch()).count();
            msg.messageType = 0;

            sendBuffer_lock.lock();
            sendBuffer.push(msg);
            sendBuffer_lock.unlock();
        }
    }
    waitSetup_lock.unlock();
}

// setup TCP connection with other servers
int setupListener(string port) {
    int sockfd = -1;
    int addrInfo = -1;
    int tmp = 1;
    struct addrinfo hints, *servinfo, *p;

    // use a struct named hints to make configuration of the socket programming
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;  // stream : TCP connections
	hints.ai_flags = AI_PASSIVE;

    // getaddrinfo(host, port, hints, result_lists)
    if ((addrInfo = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrInfo));
		return 1;
	}

    for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("setup listener: socket");
			continue;
		}

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) == -1) {
            perror("setup listener: address reuse");
			continue;
        }
        //bind the welcome socket with ip_address
		if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("setup listener: bind");
			continue;
		}

        break;
	}

    if (p == NULL) {
		fprintf(stderr, "setup listener: failed to bind socket\n");
		return -1;
	}

    // release the servinfo linkedlist
	freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        cout<<"listen fail"<<endl;
        return -1;
    }

    return sockfd;
}

void* localSender(void* param) {
    waitSetup_lock.lock();
    string identifier = *(string*) param;
    vector<string> node2sendFd;

    // set up connection

    // node2Configuration[nodeId] = nodeConfig(nodeId, address, portId);
    for (map<string, nodeConfig>::iterator it = node2Configuration.begin(); it != node2Configuration.end(); it++) {
        if (it->first == identifier)
            continue;

        int sockfd;
        int rv;
        struct addrinfo hints, *servinfo, *p = NULL;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo(it->second.address.c_str(), it->second.port.c_str(), &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            exit(1);
        }

        p = servinfo;
        // in while loop, get the first ip_address struct that could be used
        while (true) {
           if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("client: socket");
            } else if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                // perror("client: connect");
            } else {
                break;
            }

            p = p->ai_next == NULL ? servinfo : p->ai_next;
        }

        if (p == NULL) {
            fprintf(stderr, "client: failed to connect\n");
            exit(2);
        }

        freeaddrinfo(servinfo);
        sockfdVec[serverName2ID[it->first]] = sockfd;
        
    }
    // sockVec_lock.unlock();

    // cout<<identifier<<" finish connection"<<endl;
    timestamps = 0; // like lamport timestamp
    char charMessage[sizeof(message)];

    waitSetup_lock.unlock();
    // sending in while loop
    while(true) {
        while (!sendBuffer.empty()) {
            sendBuffer_lock.lock();
            message mess = sendBuffer.front();
            sendBuffer.pop();
            sendBuffer_lock.unlock();
            // print("[sending] " + to_string(mess.amount));
            memset(charMessage, 0, sizeof(message));

            if (mess.messageType == 0) {
                mess.messServerID = serverName2ID[identifier];
                mess.proposalServerID = serverName2ID[identifier];
                mess.priority = timestamps;
                timestamps++;
                // add into deliverbuffer
                deliverBuffer_lock.lock();
                deliverBuffer.push(mess);




                // what is deliver hash for
                deliverHash[mess.time][mess.proposalServerID] = make_pair(mess.priority, mess.messServerID);
                printf_lock.lock();
                printMsg("New:   ", mess);
                printf_lock.unlock();
                deliverBuffer_lock.unlock();

                inProcessMsg_lock.lock();
                // inProcessMsg only contains message sending from the local host
                inProcessMsg[mess.time] = make_pair(0, mess);
                inProcessMsg_lock.unlock();
            } else if (mess.messageType == 1) {
                mess.messServerID = serverName2ID[identifier];
                mess.priority = timestamps;
                timestamps++;

                // deliverBuffer_lock.lock();
                // deliverBuffer.push(mess);
                // deliverHash[mess.time][mess.proposalServerID] = make_pair(mess.priority, mess.messServerID);
                // printf_lock.lock();
                // printMsg("Reply: ", mess);
                // printf_lock.unlock();
                // deliverBuffer_lock.unlock();
            }

            memcpy(charMessage, &mess, sizeof(message));

            // sockVec_lock.lock();
            // broadcast decision to all servers
            if (mess.messageType != 1) {
                for (unordered_map<int, int>::iterator it = sockfdVec.begin(); it != sockfdVec.end(); it++) {
                    // cout<<"in if "<<it->first<<"\t"<<connectedServer<<endl;
                    if (!((1 << it->first) & connectedServer)) {
                        continue;
                    }
                    send(it->second, (void *) charMessage, sizeof(message), 0);
                }
            } else {
                // only return the msg to proposed server
                // cout<<"in else"<<mess.proposalServerID<<"\t"<<connectedServer<<endl;
                if ((1 << mess.proposalServerID) & connectedServer) {
                    send(sockfdVec[mess.proposalServerID], (void *) charMessage, sizeof(message), 0);
                }
            }
            // sockVec_lock.unlock();
        }
    }
}

static int acceptNewFD(struct pollfd* pofds, int index, int listener) {
    int sockfd = -1;

    if ((sockfd = accept(listener, (struct sockaddr*)NULL, NULL)) < 0) {
        perror("accept error");
    } else {
        if (index + 1 == MAX_FD_SIZE) {
            perror("Too much fd!");
        } else {
            index++;
            pofds[index].fd = sockfd;
            pofds[index].events = POLLIN;
        }
    }

    return sockfd;
}

static int deleteFD(struct pollfd* pofds, int i, int index) {
    if (fd2ID.find(pofds[i].fd) != fd2ID.end()) {
        if (lastMsgs[fd2ID[pofds[i].fd]].messageType == 2) {
            message msg = lastMsgs[fd2ID[pofds[i].fd]];
            // current server received the msg from broken server, type == 3 means that 
            msg.messageType = 3;
            sendBuffer_lock.lock();
            sendBuffer.push(msg);
            sendBuffer_lock.unlock();
        }

        connectedServer -= 1 << fd2ID[pofds[i].fd];
        ID2fd.erase(fd2ID[pofds[i].fd]);
        fd2ID.erase(pofds[i].fd);
    }

    close(pofds[i].fd);
    pofds[i].fd = pofds[index].fd;
    pofds[index].fd = -1;
    return 1;
}

static int produceReplyMessage(message& msg, int fd) {
    message newMsg = msg;
    // unorder_map find == end means the value does not exist
    if (fd2ID.find(fd) == fd2ID.end()) {
        // connectedServer += 1 << msg.proposalServerID;
        fd2ID[fd] = msg.proposalServerID;
        ID2fd[msg.proposalServerID] = fd;
    }

    newMsg.messageType = 1;
    newMsg.messServerID = thisID;

    sendBuffer_lock.lock();
    sendBuffer.push(newMsg);
    sendBuffer_lock.unlock();

    deliverBuffer_lock.lock();
    deliverBuffer.push(newMsg);
    deliverHash[newMsg.time][newMsg.proposalServerID] = make_pair(newMsg.priority, newMsg.messServerID);
    deliverBuffer_lock.unlock();

    return 1;
}

static int printAcc() {
    string result = "BALANCES ";

    for (map<string, int>::iterator it = accounts.begin(); it != accounts.end(); it++) {
        result += it->first + ": " + to_string(it->second) + "  ";
    }

    cout<<result<<endl;
    // ofstream outFile;
    // outFile.open("accout_" + to_string(thisID) + ".txt", ios_base::app);
    // outFile<<result<<endl;

    return 1;
}

static int deliverMsg(message &msg) {
    lastMsgs[msg.proposalServerID] = msg; // maintains the last msg from other servers

    deliverBuffer_lock.lock();

    printf_lock.lock();
    printMsg("PushPQ: " , msg);
    
    deliverHash[msg.time].erase(msg.proposalServerID);
    if (deliverHash[msg.time].size() == 0) {
        deliverHash.erase(msg.time);
    }

    deliverBuffer.push(msg);

    while (!deliverBuffer.empty()) {
        printDB("");
        message topMsg = deliverBuffer.top();
        if (topMsg.messageType == 2 || topMsg.messageType == 3) {
            // check whether the transaction is valid
            accounts_lock.lock();
            if (topMsg.transactionType == 1 && accounts[topMsg.transferer] < topMsg.amount) {
                ;
            } else {
                if (topMsg.transactionType == 0) {
                    accounts[topMsg.transferer] += topMsg.amount;
                } else {
                    accounts[topMsg.transferer] -= topMsg.amount;
                    accounts[topMsg.receiver] += topMsg.amount;
                }
                uint64_t time = chrono::duration_cast<chrono::nanoseconds>(
                        chrono::system_clock::now().time_since_epoch()).count();
                
                // string tmp = ID2ServerName[topMsg.proposalServerID] + " " + to_string(topMsg.time) + " " + to_string(time);
                // cout<<tmp<<endl;
                printDB();
                // cout<<topMsg.messageType<<" "<<topMsg.priority<<" "<<topMsg.messServerID<<" "<<topMsg.proposalServerID<<endl;
                printAcc();
            }
            accounts_lock.unlock();
            deliverBuffer.pop();
        } else if ((topMsg.messageType <= 1 && 
                  (deliverHash.find(topMsg.time) == deliverHash.end() ||
                   deliverHash[topMsg.time].find(topMsg.proposalServerID) == deliverHash[topMsg.time].end()))||
                  (((1 << topMsg.proposalServerID) & connectedServer) == 0 && disconnectHelper[topMsg.proposalServerID] == connectedServer)
) {
            deliverBuffer.pop();
        } else {
            break;
        }
    }

    // printDB("2=> ");
    printf_lock.unlock();
    deliverBuffer_lock.unlock();

    return 1;
}

static int serverFailureHandler(message& msg, int fd) {
    if (fd2ID.find(fd) != fd2ID.end()) {
        disconnectHelper[msg.proposalServerID] |= (1 << fd2ID[fd]);
    }

    if (lastMsgs[msg.proposalServerID].time < msg.time) {
        deliverMsg(msg);

        message newMsg = msg;
        newMsg.messageType = 4;
        sendBuffer_lock.lock();
        sendBuffer.push(newMsg);
        sendBuffer_lock.unlock();
    }
}

static int produceDecisionMessage(message& msg, int fd) {
    if (fd2ID.find(fd) == fd2ID.end()) {
        fd2ID[fd] = msg.messServerID;
        ID2fd[msg.messServerID] = fd;
    }

    inProcessMsg_lock.lock();
    // inProcessMsg[msg.time].second.messageType = 2;

// unordered_map<uint64_t, pair<int, message> > inProcessMsg;  int records how many msg that servers received
    inProcessMsg[msg.time].first += 1 << msg.messServerID;

    if (inProcessMsg[msg.time].second.priority < msg.priority) {
        inProcessMsg[msg.time].second.priority = msg.priority;
        inProcessMsg[msg.time].second.messServerID = msg.messServerID;
    } else if (inProcessMsg[msg.time].second.priority == msg.priority &&
               inProcessMsg[msg.time].second.messServerID < msg.messServerID) {
        inProcessMsg[msg.time].second.messServerID = msg.messServerID;
    }

    if ((inProcessMsg[msg.time].first & connectedServer) == connectedServer) {
        inProcessMsg[msg.time].second.messageType = 2;
        sendBuffer_lock.lock();
        sendBuffer.push(inProcessMsg[msg.time].second);
        sendBuffer_lock.unlock();

        deliverMsg(inProcessMsg[msg.time].second);

        inProcessMsg.erase(msg.time);
    }
    inProcessMsg_lock.unlock();

    return 1;
}

void* localReceiver(void* param) {
    recvParam parameter = *(recvParam*)param;

    // how many fds could support poll
    struct pollfd pofds[MAX_FD_SIZE];
    int pofds_index = 0;

    // set initial to -1
    for (int i = 0;i < MAX_FD_SIZE; i++) {
        pofds[i].fd = -1;
    }

// struct pollfd {
//     int   fd;         /* file descriptor */
//     short events;     /* requested events */
//     short revents;    /* returned events */
// };
    // all the fd in pofds are receive socket
    pofds[pofds_index].fd = parameter.listener;
    pofds[pofds_index].events = POLLIN;

    // start receiving messages
    char* buf[sizeof(message)];
    int recvByte = 0;
    while (true) {
        int cntPollEvents = poll(pofds, pofds_index+1, -1);

        if (cntPollEvents < 0) {
            perror("poll error");
            exit(3);
        } else if (cntPollEvents == 0) {
            continue;
        }
        // poll all fds
        for (int i = 0; i <= pofds_index; i++) {
            if(!(pofds[i].revents & POLLIN)) {
                continue;
            }

            if (pofds[i].fd == parameter.listener) {
                // print("Get connection");
                acceptNewFD(pofds, pofds_index, parameter.listener);
                pofds_index++;
            } else {
                // print("Get a message");
                memset(buf, '\0' ,sizeof(message));
                recvByte = recv(pofds[i].fd, buf, sizeof(message), 0);

                if (recvByte < 0) {
                    perror("recv message error\n");
                } else if (recvByte == 0) {
                    deleteFD(pofds, i, pofds_index);
                    pofds_index--;
                } else {
                    // print("Reading message");
                    message currMess;
                    memset(&currMess, '\0', sizeof(message));
                    memcpy(&currMess, buf, sizeof(message));

                    switch (currMess.messageType) {
                        case 0:
                            // print("case 0");
                            produceReplyMessage(currMess, pofds[i].fd);
                            break;
                        case 1:
                            // print("case 1");
                            produceDecisionMessage(currMess, pofds[i].fd);
                            break;
                        case 2:
                            // print("case 2");
                            deliverMsg(currMess);
                            break;
                        case 3:
                            // print("case 3");
                            serverFailureHandler(currMess, pofds[i].fd);
                            break;
                        case 4:
                            // print("case 4");
                            if (lastMsgs[currMess.proposalServerID].time < currMess.time) {
                                deliverMsg(currMess);
                            }
                        default:
                            print("a strange message type");
                            break;
                    }
                }
            }
        }
    }
}
