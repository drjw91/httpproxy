#pragma once
#include <vector>

class CDnsParse
{
public:
    CDnsParse();
    virtual ~CDnsParse();

    int parsedns(char* szHost, unsigned int nipremote, unsigned short uportremote);

protected:
    int parsednsremote(char* szHost, unsigned int nipremote, unsigned short uportremote);
    int parsednslocal(char* szHost);
};

class CHttpTun
{
public:
    CHttpTun(SOCKET s, unsigned int nipremote, unsigned int uportremote,unsigned int uportremoteudp, BOOL bHandle = TRUE);
    virtual ~CHttpTun();

    BOOL SendData();
    void SetSendData(char* buffer, int nSize);

    BOOL IsCanSend();
    BOOL IsNeedSend();

    BOOL IsConnect();
    int Connect( char* szHost, unsigned short port );

    int RecvData(char* buffer, int nbuffersize);
    SOCKET m_s;
protected:
    BOOL m_bHandle;
    char m_SendCache[4096];
    int  m_nSendIndex;
    int  m_nSendSize;
    BOOL m_bIsConnect;

    unsigned int m_nipremote;
    unsigned short m_uportremote;
    unsigned short m_uportremoteudp;
};

class CHttpProxyServer
{
public:
    CHttpProxyServer();
    virtual ~CHttpProxyServer();

    int  Init(unsigned int nip, unsigned short uport, unsigned int nipremote, unsigned short uportremote, unsigned short uportremoteudp, unsigned int nTimeout = 1000);
    void Uninit();
    BOOL IsRunning();
    int  SetProxy(char* szip, unsigned short port);

protected:
    typedef struct tag_SOCKETCLIENT
    {
        SOCKET sClient;
        SOCKADDR_IN addr;
        unsigned int nipremote;
        unsigned short uportremote;
        unsigned short uportremoteudp;
        void *lpthis;
    }SOCKETCLIENT, *LPSOCKETCLIENT;

    static unsigned int __stdcall s_AcceptThread(void* param);
    unsigned int AcceptThread();

    static unsigned int __stdcall s_TunThread(void* param);
    unsigned int TunThread(SOCKETCLIENT &clientinfo);

    BOOL accept_client(SOCKET s, SOCKADDR_IN* addr);

    unsigned int m_nip;
    unsigned short m_uport;
    unsigned int m_nipremote;
    unsigned short m_uportremote;
    unsigned short m_uportremoteudp;

    HANDLE m_hExit;
    HANDLE m_hUpdate;
    HANDLE m_hWorkThread;
    volatile LONG m_threadcnts;
};