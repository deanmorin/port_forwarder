#include <arpa/inet.h>
#include <boost/program_options.hpp>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sstream>
#include "badbaseexception.hpp"
#include "eventbase.hpp"
#include "forwardinginfo.hpp"
#include "network.hpp"
#include "tpool.h"
namespace po = boost::program_options;
using namespace dm;

#define DFLT_THREADS    16
#define DFLT_QUEUE      4096
#define DFLT_PORT       32000
#define DFLT_RULESFILE  "forward.csv"
#define LISTEN_BACKLOG  65535

struct clientStats
{
    std::string hostName;
    int port;
    int requestsRecv;
    unsigned long dataSent;
};


/**
 * Perform the initialization required to use the libevent library.
 *
 * @author Dean Morin
 * @param method The desired event method to use.
 * @return The initialized event base. This is heap allocated and so the caller
 *      must call delete on it later.
 */
EventBase* initlibEvent(const char* method);
evutil_socket_t listenSock(const int port);
void runServer(EventBase* eb, const int numWorkerThreads, 
        const int maxQueueSize);
void updateClientStats(evutil_socket_t fd, int data);
void forwardingRules(std::string& fileName);

/**
 * Increment the count of connected clients. Thread safe.
 * 
 * @author Dean Morin
 * @param sa The address info on the new connection.
 */
void incrementClients(evutil_socket_t fd, struct sockaddr_in* sa);

/**
 * Decrement the count of connected clients. Thread safe.
 * 
 * @author Dean Morin
 * @param fd The socket that is being closed.
 */
void decrementClients(evutil_socket_t fd);

pthread_mutex_t clientMutex;
pthread_mutex_t jobMutex;
int clientCount;
int maxClientCount;
std::map<evutil_socket_t, struct clientStats> clientStats;
std::map<int, ForwardingInfo> clients;

/**
 * A server intended to test the differences in efficiency between the various
 * event handling methods.
 *
 * @author Dean Morin
 */
int main(int argc, char** argv)
{
    int opt = 0;
    std::string sopt = "";
    int threads = 0;
    int queue = 0;
    std::string rulesFile = "";
    EventBase* eb = NULL;
    std::string method = "";

    po::options_description desc("Allowed options");
    desc.add_options()
        ("rules-file,f",
                po::value<std::string>(&sopt)->default_value(DFLT_RULESFILE),
                "the file with the forwarding rules")
        ("kqueue,k", "use kqueue()")
        ("epoll,e", "use epoll()")
        ("select,s", "use select()")
        ("poll,p", "use poll()")
        ("thread-pool,T", po::value<int>(&opt)->default_value(DFLT_THREADS),
                "number of threads in the thread pool")
        ("max-queue,M", po::value<int>(&opt)->default_value(DFLT_QUEUE),
                "max number of jobs in the pool queue")
        ("help", "show this message")
    ;

    po::variables_map vm;
    try 
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } 
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "\tuse --help to see program options\n";
        return 1;
    }

    rulesFile = vm["rules-file"].as<std::string>();
    threads = vm["thread-pool"].as<int>();
    queue = vm["max-queue"].as<int>();
    
    if (pthread_mutex_init(&clientMutex, NULL))
    {
        std::cerr << "Error creating mutex\n";
        exit(1);
    }
    if (pthread_mutex_init(&jobMutex, NULL))
    {
        std::cerr << "Error creating mutex\n";
        exit(1);
    }
    clientCount = 0;
    maxClientCount = 0;
    
    if (vm.count("help"))
    {
        std::cout << desc << "\n";
    }
    else if (vm.count("kqueue"))
    {
        method = "kqueue";
    }
    else if (vm.count("epoll"))
    {
        method = "epoll";
    }
    else if (vm.count("select"))
    {
        method = "select";
    }
    else if (vm.count("poll"))
    {
        method = "poll";
    }

    forwardingRules(rulesFile);
    eb = initlibEvent(method.c_str());
    runServer(eb, threads, queue);

    return 0;
}


void forwardingRules(std::string& fileName)
{
    std::ifstream in(fileName.c_str());
    std::string line = "";                    
    int forwarderPort = -1;
    std::string serverName = "";
    int serverPort = -1;

    if (!in)
    {
        std::cerr << "Unable to open \"" << fileName << "\"\n";
        exit(1);
    }
    
    while (getline(in, line))
    {             
        char lineNonConst[128];
        strcpy(lineNonConst, line.c_str());
        forwarderPort = atoi(strtok(lineNonConst, ","));
        serverName = strtok(NULL, ",");
        serverPort = atoi(strtok(NULL, ","));

        clients[forwarderPort].serverName_ = serverName;
        clients[forwarderPort].serverPort_ = serverPort;
    }
}


EventBase* initlibEvent(const char* method)
{
    try
    {
        EventBase* eb = new EventBase(method);
        std::cout << "Using: " << eb->getMethod() << "\n";
        return eb;
    }
    catch (const BadBaseException& e)
    {
        int i = 0;
        const char** methods = EventBase::getAvailableMethods();

        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "\tThe available event bases are:\n";

        for (i = 0; methods[i] != NULL; i++)
        {
            std::cerr << "\t - " << methods[i] << "\n";
        }
        exit(1);
    }
    catch (...)
    {
        std::cerr << "Error: pthreads are not available on this machine\n";
        exit(1);
    }
}


/**
 * Display the maximum number of clients that were connected at one time, then
 * shut down the server. Initiated by ctrl-c.
 *
 * @author Dean Morin
 */
void shutDown(int)
{
    std::cout << "\nHighest number of simultaneous connections: " 
              << maxClientCount << "\n\n";

    pthread_mutex_lock(&clientMutex);

    std::cout << "Clients still connected: \n\n";

    std::map<evutil_socket_t, struct clientStats>::iterator it;
    for (it = clientStats.begin(); it != clientStats.end(); ++it)
    {
        struct clientStats c = it->second;
        std::cout << "\tHost name:\t\t" << c.hostName << "\n"
                  << "\tPort:\t\t\t" << c.port << "\n"
                  << "\tRequests received:\t" << c.requestsRecv << "\n"
                  << "\tData sent:\t\t" << c.dataSent << "\n\n";
    }

    std::map<int, ForwardingInfo>::iterator it2;
    for (it2 = clients.begin(); it2 != clients.end(); ++it2)
    {
        ForwardingInfo c = it2->second;
        std::cout << "\tPort:        " << it2->first    << "\n"
                  << "\tServer Name: " << c.serverName_ << "\n"
                  << "\tServer Port: " << c.serverPort_ << "\n\n";
    }

    pthread_mutex_unlock(&clientMutex);

	exit(0);
}

void handleSigurg(evutil_socket_t, short, void*)
{
    std::cout << "Out of band data arrived. Probably best to just ignore it...\n";
}

/**
 * When ctrl-c is pressed and libevent is being used, this function frees the
 * listen socket, then calls shutDown().
 *
 * @param arg The struct responsible for the listening socket.
 * @author Dean Morin
 */
void handleSigint(evutil_socket_t, short, void* arg)
{
    std::list<struct evconnlistener*>* listenList
            = (std::list<struct evconnlistener*>*) arg;

    std::list<struct evconnlistener*>::iterator it;
    for (it = listenList->begin(); it != listenList->end(); ++it)
    {
        evconnlistener_free(*it);
    }

    shutDown(0);
}

void cancelJobs(tPool* tpool, struct bufferevent* bev)
{
    pthread_mutex_lock(&tpool->queueLock);

    tPoolJob* currentJob = tpool->queueHead;
    struct bufferevent* currentBev;

    while(currentJob != NULL)
    {
        currentBev = (struct bufferevent*) currentJob->arg;
        if (currentBev == bev)
        {
            currentJob->arg = NULL;
        }
        currentJob = currentJob->next;
    }

    pthread_mutex_unlock(&tpool->queueLock);
}

static void sockEvent(struct bufferevent* bev, short events, void* arg)
{
    if (events & BEV_EVENT_ERROR)
    {
        perror("Error from bufferevent");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) 
    {
        decrementClients(bufferevent_getfd(bev));

        pthread_mutex_lock(&jobMutex);

        cancelJobs((tPool*)arg, bev);
        bufferevent_free(bev);

        pthread_mutex_unlock(&jobMutex);
    }
}

void handleRequest(void* args)
{
    pthread_mutex_lock(&jobMutex);
    if (!args)
    {
        // bufferevent has been freed; this is a stale job
        pthread_mutex_unlock(&jobMutex);
        return;
    }

    struct bufferevent* bev = (struct bufferevent*) args;
    evutil_socket_t fd = bufferevent_getfd(bev);

    struct evbuffer *input = bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output(bev);
    uint32_t msgSize;

    if (evbuffer_copyout(input, &msgSize, sizeof(uint32_t)) == -1)
    {
        std::cerr << "Error: evbuffer_copyout\n";
    }

    char* buf = new char[msgSize];

    // fill the packet with random characters
    for (size_t i = 0; i < msgSize; i++)
    {
        buf[i] = rand() % 93 + 33;
    }
    evbuffer_add(output, buf, msgSize);

    pthread_mutex_unlock(&jobMutex);

    updateClientStats(fd, msgSize);
    delete[] buf;
}

static void readSock(struct bufferevent* bev, void* arg)
{
    if (tPoolAddJob((tPool*) arg, handleRequest, bev))
    {
        std::cerr << "Error adding new job to thread pool\n";
        exit(1);
    }
}

static void acceptErr(struct evconnlistener* listener, void*)
{
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    std::cerr << "Error " << err << "(" << evutil_socket_error_to_string(err)
              << ") on listening socket. Shutting down.\n";
    event_base_loopexit(base, NULL);
}

static void acceptClient(struct evconnlistener* listener, evutil_socket_t fd,
        struct sockaddr* sa, int, void* arg)
{
    int sock = -1;
    int forwarderPort = 0;
    std::string clientName = "";
    int clientPort = 0;

    incrementClients(fd, (sockaddr_in*) sa);

    struct event_base* base = evconnlistener_get_base(listener);
    struct bufferevent* bev = bufferevent_socket_new(base, fd, 
            BEV_OPT_CLOSE_ON_FREE);

    forwarderPort = ((sockaddr_in*) sa)->sin_port;

    //serverName = clients[forwarderPort].serverName_;
    //serverPort = clients[forwarderPort].serverPort_;

    //if ((sock = connectSocket(serverName, serverPort)) < 0)
    //{
        //exit(1);
    //}
    //std::cerr << socket << " : socket\n";

    bufferevent_setcb(bev, readSock, NULL, sockEvent, arg);
    bufferevent_enable(bev, EV_READ | EV_WRITE); 
}


std::list<struct evconnlistener*>* bindListeners(EventBase* eb, tPool* pool)
{
    struct evconnlistener* listener;
	struct sockaddr_in addr;
    std::list<struct evconnlistener*>* listenList
            = new std::list<struct evconnlistener*>();

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    std::map<int, ForwardingInfo>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it)
    {
        addr.sin_port = htons(it->first);
#ifdef DEBUG
        std::cout << "listening on port " << it->first << "\n";
#endif
        if (!(listener = evconnlistener_new_bind(eb->getBase(), acceptClient,
                pool, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, LISTEN_BACKLOG, 
                (struct sockaddr*) &addr, sizeof(addr))))
        {
            exit(sockError("evconnlistener_new_bind()", 0));
        }
        evconnlistener_set_error_cb(listener, acceptErr);
        listenList->push_back(listener);
    }
    return listenList;
}


void runServer(EventBase* eb, const int numWorkerThreads,
        const int maxQueueSize) 
{
    std::list<struct evconnlistener*>* listenList;

    tPool* pool = NULL;
    int blockWhenQueueFull = 1;

    if (tPoolInit(&pool, numWorkerThreads, maxQueueSize, blockWhenQueueFull))
    {
        std::cerr << "Error initializing thread pool\n";
        exit(1);
    }

    listenList = bindListeners(eb, pool);

    struct event* sigint;
    sigint = evsignal_new(eb->getBase(), SIGINT, handleSigint, listenList);
    evsignal_add(sigint, NULL);

    struct event* sigurg;
    sigurg = evsignal_new(eb->getBase(), SIGURG, handleSigurg, NULL);
    evsignal_add(sigurg, NULL);

    event_base_dispatch(eb->getBase());
    event_del(sigint);
    event_del(sigurg);
}

void updateClientStats(evutil_socket_t fd, int data)
{
    pthread_mutex_lock(&clientMutex);

    clientStats[fd].requestsRecv++;
    clientStats[fd].dataSent += data;

    pthread_mutex_unlock(&clientMutex);
}


void incrementClients(evutil_socket_t fd, struct sockaddr_in* sa)
{
    pthread_mutex_lock(&clientMutex);

    if (++clientCount > maxClientCount)
    {
        maxClientCount = clientCount;
    }
#ifdef DEBUG
    std::cout << "Clients++ " << clientCount << "\n";
#endif
    // add client to map
    clientStats[fd].hostName = inet_ntoa(sa->sin_addr);
    clientStats[fd].port = sa->sin_port;

    pthread_mutex_unlock(&clientMutex);
}


void decrementClients(evutil_socket_t fd)
{
    pthread_mutex_lock(&clientMutex);

    clientCount--;
#ifdef DEBUG
    std::cout << "Clients-- " << clientCount << "\n";
#endif
    clientStats.erase(fd);

    pthread_mutex_unlock(&clientMutex);
}
