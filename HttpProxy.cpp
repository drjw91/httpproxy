#include "StdAfx.h"
#include "HttpProxy.h"
#include "httpparser/request.h"
#include "httpparser/httprequestparser.h"
#include <ws2tcpip.h>
#include <atlstr.h>
#include <WinInet.h>
#include <process.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Wininet.lib")

CDnsParse::CDnsParse()
{

}

CDnsParse::~CDnsParse()
{

}

int CDnsParse::parsedns( char* szHost, unsigned int nipremote, unsigned short uportremote )
{
    CStringA strinfo;
    struct in_addr in;
    int nret = parsednsremote(szHost, nipremote, uportremote);
    if (nret == 0 || nret==-1)
    {
        nret = parsednslocal(szHost);
        in.s_addr = nret;
        strinfo.Format("dns:%s(%s) parse from local", szHost, inet_ntoa(in));
    }
    else
    {
        in.s_addr = nret;
        strinfo.Format("dns:%s(%s) parse from remote", szHost, inet_ntoa(in));
    }

    Log((char*)strinfo.GetString());
    return nret;
}

int CDnsParse::parsednsremote( char* szHost, unsigned int nipremote, unsigned short uportremote )
{
    return 0;
}

int CDnsParse::parsednslocal( char* szHost )
{
    int nRet = 0;
    addrinfo hints = {0};
    addrinfo* res = NULL;
    addrinfo* cur = NULL;

    sockaddr_in* addr = NULL;

    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    hints.ai_socktype = SOCK_STREAM;

    if (0 == getaddrinfo(szHost, NULL, &hints, &res))
    {
        for (cur = res; cur != NULL; cur = cur->ai_next)
        {
            addr = (struct sockaddr_in *)cur->ai_addr;
            if (addr)
            {
                if(addr->sin_addr.S_un.S_addr == -1 || addr->sin_addr.S_un.S_addr == 0)
                {
                    continue;
                }
                nRet = addr->sin_addr.S_un.S_addr;
                break;
            }
        }
        freeaddrinfo(res);
        res = NULL;
    }
    return nRet;
}

CHttpTun::CHttpTun( SOCKET s, unsigned int nipremote, unsigned int uportremote,unsigned int uportremoteudp, BOOL bHandle /*= TRUE*/ )
: m_s(s)
, m_bHandle(bHandle)
, m_nSendIndex(0)
, m_nSendSize(-1)
, m_bIsConnect(FALSE)
, m_nipremote(nipremote)
, m_uportremote(uportremote)
, m_uportremoteudp(uportremoteudp)
{

}

CHttpTun::~CHttpTun()
{
    if (m_bHandle)
    {
        if (m_s != INVALID_SOCKET)
        {
            ::closesocket(m_s);
            m_s = INVALID_SOCKET;
        }
    }
}

BOOL CHttpTun::SendData()
{
    BOOL bRet = TRUE;
    if (m_nSendSize)
    {
        int needSend = m_nSendSize - m_nSendIndex;
        if (needSend)
        {
            int nret = send(m_s, m_SendCache + m_nSendIndex, needSend, 0);
            if (nret == SOCKET_ERROR)
            {
                if (WSAEWOULDBLOCK == ::WSAGetLastError())
                {
                    m_nSendIndex += needSend;
                }
                else
                {
                    bRet = FALSE;
                }
            }
            else
            {
                m_nSendIndex += nret;
            }
        }
    }
    return bRet;
}

void CHttpTun::SetSendData( char* buffer, int nSize )
{
    m_nSendIndex = 0;
    m_nSendSize = nSize;
    memcpy(m_SendCache, buffer, m_nSendSize);
}

BOOL CHttpTun::IsCanSend()
{
    return m_nSendIndex >= m_nSendSize;
}

BOOL CHttpTun::IsNeedSend()
{
    if (m_nSendSize <= 0)
    {
        return FALSE;
    }

    return m_nSendSize > m_nSendIndex;
}

BOOL CHttpTun::IsConnect()
{
    return m_bIsConnect;
}

int CHttpTun::Connect( char* szHost, unsigned short port )
{
    int nRet = -1;

    CDnsParse dnsParse;
    sockaddr_in addrremote = {0};

    addrremote.sin_port=port;
    addrremote.sin_family=AF_INET;
    addrremote.sin_addr.S_un.S_addr=dnsParse.parsedns(szHost, m_nipremote, m_uportremoteudp);

    if (addrremote.sin_addr.S_un.S_addr == 0 || addrremote.sin_addr.S_un.S_addr == -1)
    {
        nRet = 2;
        goto Exit0;
    }

    {
        int nret = -1;
        u_long iMode = 1;
        nret = connect(m_s, (const sockaddr*)&addrremote, sizeof(addrremote));
        if(nret == SOCKET_ERROR)
        {
            nRet = 3;
            goto Exit0;
        }

        if (SOCKET_ERROR == ioctlsocket(m_s, FIONBIO, &iMode))
        {
            nRet = 7;
            goto Exit0;
        }
        nRet = 0;
        m_bIsConnect = TRUE;
    }
Exit0:
    return nRet;
}

int CHttpTun::RecvData( char* buffer, int nbuffersize )
{
    return recv(m_s, buffer, nbuffersize, 0);
}

CHttpProxyServer::CHttpProxyServer()
: m_nip(-1)
, m_uport(0)
, m_nipremote(-1)
, m_uportremote(0)
, m_uportremoteudp(0)
, m_hWorkThread(NULL)
, m_hExit(NULL)
, m_hUpdate(NULL)
, m_threadcnts(0)
{

}

CHttpProxyServer::~CHttpProxyServer()
{

}

int CHttpProxyServer::Init( unsigned int nip, unsigned short uport, unsigned int nipremote, unsigned short uportremote, unsigned short uportremoteudp, unsigned int nTimeout /*= 1000*/ )
{
    int nret = -1;
    Uninit();

    if (nip == 0 || nip == -1 || uport == 0)
    {
        nret=1;
        goto Exit0;
    }

    if (m_hUpdate = ::CreateEvent(NULL, FALSE, FALSE, NULL), m_hUpdate==NULL)
    {
        nret=2;
        goto Exit0;
    }

    if (m_hExit = ::CreateEvent(NULL, TRUE, FALSE, NULL), m_hExit==NULL)
    {
        nret=3;
        goto Exit0;
    }

    m_nip = nip;
    m_uport = uport;
    m_nipremote = nipremote;
    m_uportremote = uportremote;
    m_uportremoteudp = uportremoteudp;

    if (m_hWorkThread = (HANDLE)_beginthreadex(NULL, 0, CHttpProxyServer::s_AcceptThread, (void*)this, 0, NULL), m_hWorkThread==NULL)
    {
        nret=4;
        goto Exit0;
    }

    m_threadcnts = 0;

    if (::WaitForSingleObject(m_hUpdate, nTimeout)!=WAIT_OBJECT_0)
    {
        nret=5;
        goto Exit0;
    }

    nret = 0;
Exit0:
    if(nret != 0)
    {
        Uninit();
    }
    return nret;
}

void CHttpProxyServer::Uninit()
{
    if (m_hExit)
    {
        ::SetEvent(m_hExit);
    }

    if (m_hWorkThread)
    {
        if (WAIT_TIMEOUT == WaitForSingleObject(m_hWorkThread, 4*1000))
        {
            ::TerminateThread(m_hWorkThread, -2);
        }
        ::CloseHandle(m_hWorkThread);
        m_hWorkThread = NULL;
    }

    if (m_hExit)
    {
        ::CloseHandle(m_hExit);
        m_hExit = NULL;
    }

    if (m_hUpdate)
    {
        ::CloseHandle(m_hUpdate);
        m_hUpdate = NULL;
    }
}

BOOL CHttpProxyServer::IsRunning()
{
    return m_hWorkThread!=NULL;
}

int CHttpProxyServer::SetProxy( char* szip, unsigned short port )
{
    DWORD dwType = REG_SZ;
    char szValue[2048 + 2] = { 0 };
    DWORD dwValueSize = sizeof(szValue);

    LONG lRet = ::SHGetValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", "ProxyServer", &dwType, szValue, &dwValueSize );
    if (lRet == ERROR_SUCCESS )
    {
        CStringA strinfo = szValue;
        if (strinfo.Find("http=") == -1)
        {
        }

        DWORD dwWord = 0;
        if (port != 0)
        {
            dwWord=1;
            strinfo.Format("%s:%d", szip, port);
        }
        else
        {
            strinfo.Empty();
        }

        ::SHSetValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", "ProxyServer", REG_SZ, strinfo.GetString(), strinfo.GetLength()+1);
        ::SHSetValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", "ProxyEnable", REG_DWORD, &dwWord, sizeof(dwWord));

        ::InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    }
    return 0;
}

unsigned int __stdcall CHttpProxyServer::s_AcceptThread( void* param )
{
    int nret = -1;
    __try
    {
        nret = ((CHttpProxyServer*)param)->AcceptThread();
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        nret = -2;
    }
    return nret;
}

unsigned int CHttpProxyServer::AcceptThread()
{
    int nret = -1;
    BOOL bInitWSA = FALSE;
    u_long iMode = 1;
    SOCKET sserver = INVALID_SOCKET;
    WSAData wsaData = {0};
    timeval vtimeout = { 1,0 };

    SOCKADDR_IN serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = m_nip;
    serverAddr.sin_port = m_uport;

    if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        goto Exit0;
    }

    bInitWSA = TRUE;
    if (sserver = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), sserver == INVALID_SOCKET)
    {
        nret = 1;
        goto Exit0;
    }

    if (SOCKET_ERROR == ioctlsocket(sserver, FIONBIO, &iMode))
    {
        nret = 2;
        goto Exit0;
    }

    if (SOCKET_ERROR == bind(sserver, (const sockaddr*)&serverAddr, sizeof(serverAddr)))
    {
        nret = 3;
        goto Exit0;
    }

    if (SOCKET_ERROR == ::listen(sserver, SOMAXCONN))
    {
        nret = 4;
        goto Exit0;
    }

    ::SetEvent(m_hUpdate);

    while(::WaitForSingleObject(m_hExit, 0) == WAIT_TIMEOUT)
    {
        fd_set socketSetRead;
        FD_ZERO(&socketSetRead);
        FD_SET(sserver, &socketSetRead);

        int iRetCode = select(0, &socketSetRead, NULL, NULL, &vtimeout);
        if (iRetCode > 0)
        {
            if (FD_ISSET(sserver, &socketSetRead))
            {
                SOCKADDR_IN ClientAddr = {0};
                int nlen = sizeof(ClientAddr);
                SOCKET sockClient = accept(sserver, (sockaddr *)&ClientAddr, &nlen);
                if (sockClient != INVALID_SOCKET)
                {
                    if(!accept_client(sockClient, &ClientAddr))
                    {
                        closesocket(sockClient);
                        sockClient = INVALID_SOCKET;
                    }
                }
            }
        }
    }
    ::SetEvent(m_hExit);
Exit0:
    if(sserver!=INVALID_SOCKET)
    {
        closesocket(sserver);
        sserver = INVALID_SOCKET;
    }

    if (nret != 0)
    {
        ::SetEvent(m_hUpdate);
    }

    if(bInitWSA)
    {
        ::WSACleanup();
    }
    return nret;
}

unsigned int __stdcall CHttpProxyServer::s_TunThread( void* param )
{
    int nret = -1;
    __try
    {
        LPSOCKETCLIENT lpInfo = (LPSOCKETCLIENT)param;
        if(lpInfo)
        {
            SOCKETCLIENT info = *lpInfo;
            delete lpInfo;
            lpInfo = NULL;
            nret = ((CHttpProxyServer*)info.lpthis)->TunThread(info);

            if (info.sClient != INVALID_SOCKET)
            {
                closesocket(info.sClient);
                info.sClient = INVALID_SOCKET;
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        nret = -2;
    }
    return nret;
}

#define LOCAL_CLOSE (1)
#define REMOTE_CLOSE (2)
#define ALL_CLOSE (LOCAL_CLOSE|REMOTE_CLOSE)

unsigned int CHttpProxyServer::TunThread( SOCKETCLIENT &clientinfo )
{
    timeval vtimeout = { 1,0 };
    CHttpTun slocal(clientinfo.sClient, clientinfo.nipremote, clientinfo.uportremote, clientinfo.uportremoteudp, FALSE);
    CHttpTun sremote(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), clientinfo.nipremote, clientinfo.uportremote, clientinfo.uportremoteudp);

    volatile DWORD dwClsFlag = 0;
    volatile DWORD dwTimeCls = 0;

    InterlockedIncrement(&m_threadcnts);
    LogThread(m_threadcnts);

    if (sremote.m_s != INVALID_SOCKET && slocal.m_s != INVALID_SOCKET)
    {
        while(::WaitForSingleObject(m_hExit, 0) == WAIT_TIMEOUT)
        {
            fd_set socketSetRead, socketSetWrite;
            FD_ZERO(&socketSetRead);
            FD_ZERO(&socketSetWrite);

            if (0 == (dwClsFlag&LOCAL_CLOSE))
            {
                FD_SET(slocal.m_s, &socketSetRead);
            }

            if (slocal.IsNeedSend())
            {
                FD_SET(slocal.m_s, &socketSetWrite);
            }

            if (sremote.IsConnect())
            {
                if (0 == (dwClsFlag&REMOTE_CLOSE))
                {
                    FD_SET(sremote.m_s, &socketSetRead);
                }

                if(sremote.IsNeedSend())
                {
                    FD_SET(sremote.m_s, &socketSetWrite);
                }
            }

            int iRetCode = select(0, &socketSetRead, &socketSetWrite, NULL, &vtimeout);
            if (iRetCode > 0)
            {
                if (FD_ISSET(slocal.m_s, &socketSetRead))
                {
                    if (!sremote.IsConnect() || sremote.IsCanSend())
                    {
                        char szbuffer[4096]={0};
                        int nret = slocal.RecvData(szbuffer, sizeof(szbuffer)-1);
                        if(nret == SOCKET_ERROR)
                        {
                            break;
                        }

                        if (nret == 0)
                        {
                            dwClsFlag |= LOCAL_CLOSE;
                            shutdown(sremote.m_s, SD_SEND);
                            if (dwClsFlag == ALL_CLOSE)
                            {
                                break;
                            }
                        }
                        else
                        {
                            if (!sremote.IsConnect())
                            {
                                char szhost[1024] = {0};
                                unsigned int uport = 0;
                                int nconnect = -1;

                                httpparser::Request request;
                                httpparser::HttpRequestParser parser;

                                parser.parse(request, szbuffer, szbuffer + nret);
                                std::string strhost = request.gethost();

                                if (strhost.empty())
                                {
                                    Log("first connect error");
                                    break;
                                }

                                BOOL bConnectOK = (2 == sscanf_s(strhost.c_str(), "%[^:]:%d", szhost, sizeof(szhost)-1, &uport) && (nconnect = sremote.Connect(szhost, htons(uport)), nconnect == 0));
                                if(bConnectOK)
                                {
                                    if(stricmp(request.method.c_str(), "CONNECT") == 0)
                                    {
                                        slocal.SetSendData("HTTP/1.1 200 Connection Established\r\n\r\n", strlen("HTTP/1.1 200 Connection Established\r\n\r\n"));
                                    }
                                    else
                                    {
                                        sremote.SetSendData(szbuffer, nret);
                                    }
                                }

                                CStringA strlog;
                                strlog.Format("%s %s return:%d", strhost.c_str(), request.method.c_str(), nconnect);
                                Log((char*)strlog.GetString());

                                if (!bConnectOK)
                                {
                                    break;
                                }
                            }
                            else
                            {
                                sremote.SetSendData(szbuffer, nret);
                            }
                        }
                    }
                }

                if (FD_ISSET(slocal.m_s, &socketSetWrite))
                {
                    if(!slocal.SendData())
                    {
                        break;
                    }
                }

                if (FD_ISSET(sremote.m_s, &socketSetRead))
                {
                    if (slocal.IsCanSend())
                    {
                        char szbuffer[4096] = {0};
                        int nret = sremote.RecvData(szbuffer,  sizeof(szbuffer)-1);

                        if(nret == SOCKET_ERROR)
                        {
                            break;
                        }

                        if (nret == 0)
                        {
                            dwClsFlag |= REMOTE_CLOSE;
                            shutdown(slocal.m_s, SD_SEND);
                            if (dwClsFlag == ALL_CLOSE)
                            {
                                break;
                            }
                        }
                        else
                        {
                            slocal.SetSendData(szbuffer, nret);
                        }
                    }
                }

                if (FD_ISSET(sremote.m_s, &socketSetWrite))
                {
                    if(!sremote.SendData())
                    {
                        break;
                    }
                }
            }
            else if(iRetCode == 0)
            {
                if (dwClsFlag == ALL_CLOSE)
                {
                    break;
                }
                else if(dwClsFlag > 0)
                {
                    dwTimeCls++;
                    if (dwTimeCls >= 5)
                    {
                        break;
                    }
                }
            }
            else
            {
                break;
            }
        }
    }
    InterlockedDecrement(&m_threadcnts);
    LogThread(m_threadcnts);
    return 0;
}

BOOL CHttpProxyServer::accept_client( SOCKET s, SOCKADDR_IN* addr )
{
    BOOL bRet = FALSE;
    LPSOCKETCLIENT info = new(std::nothrow) SOCKETCLIENT;
    if(info)
    {
        info->sClient = s;
        if(addr)
        {
            info->addr = *addr;
        }
        info->nipremote = m_nipremote;
        info->uportremote = m_uportremote;
        info->uportremoteudp = m_uportremoteudp;
        info->lpthis = this;

        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, CHttpProxyServer::s_TunThread, (void*)info, 0, NULL);
        if(hThread)
        {
            bRet = TRUE;
            ::CloseHandle(hThread);
            hThread = NULL;
        }
        else
        {
            delete info;
            info = NULL;
        }
    }
    return bRet;
}