// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "atlddx.h"
#include "resource.h"
#include <atlframe.h>
#include "HttpProxy.h"

class CMainDlg : public CDialogImpl<CMainDlg>, public CUpdateUI<CMainDlg>,
		public CMessageFilter, public CIdleHandler, public CWinDataExchange<CMainDlg>
{
public:
	enum { IDD = IDD_MAINDLG };

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CWindow::IsDialogMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		UIUpdateChildWindows();
		return FALSE;
	}

    BEGIN_DDX_MAP(CMainDlg)
        DDX_TEXT(IDC_IPADDRESS, m_strip)
        DDX_TEXT(IDC_EDIT_PORT, m_strport)
        DDX_TEXT(IDC_IPADDRESS_REMOTE, m_stripremote)
        DDX_TEXT(IDC_EDIT_PORT_REMOTE, m_strportremote)
        DDX_TEXT(IDC_EDIT_PORT_REMOTE_UDP, m_strportremoteudp)
        DDX_TEXT(IDC_EDIT_LOG, m_strdatalog)
    END_DDX_MAP()

	BEGIN_UPDATE_UI_MAP(CMainDlg)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        COMMAND_HANDLER(IDC_BUTTON_START, BN_CLICKED, OnBnClickedButtonStart)
        MESSAGE_HANDLER(WM_LOGINFO, OoLoginfo)
    END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// center the dialog on the screen
		CenterWindow();

		// set icons
		HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
		SetIcon(hIcon, TRUE);
		HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
		SetIcon(hIconSmall, FALSE);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		UIAddChildWindowContainer(m_hWnd);

        m_strip = L"127.0.0.1";
        m_strport = L"8888";

        g_HWND = m_hWnd;

        m_editdatalog = GetDlgItem(IDC_EDIT_LOG);

        DoDataExchange(FALSE);
		return TRUE;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// unregister message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->RemoveMessageFilter(this);
		pLoop->RemoveIdleHandler(this);

		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		return 0;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: Add validation code 
		CloseDialog(wID);
		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CloseDialog(wID);
		return 0;
	}

	void CloseDialog(int nVal)
	{
		DestroyWindow();
		::PostQuitMessage(nVal);
	}

    LRESULT OoLoginfo(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
    {
        switch (wParam)
        {
        case 0:
            {
                LPCSTR szinfo = (LPCSTR)lParam;
                if(szinfo)
                {
                    if (szinfo[0])
                    {
                        CStringW strinfo(szinfo);

                        if(m_strdatalog.GetLength() > 1024*16)
                        {
                            m_strdatalog.Empty();
                        }

                        m_strdatalog.AppendFormat(L"%s\r\n", strinfo.GetString());
                        DoDataExchange(FALSE, IDC_EDIT_LOG);

                        m_editdatalog.LineScroll(m_editdatalog.GetLineCount());
                    }
                    delete[] szinfo;
                    szinfo = NULL;
                }
            }
            break;
        case 1:
            {
                CStringW strinfo;
                strinfo.Format(L"当前转发线程数(%d)", lParam);
                GetDlgItem(IDC_STATIC_THREAD).SetWindowText(strinfo.GetString());
            }
            break;
        }
        return 0;
    }

    CHttpProxyServer m_proxy;
    WTL::CEdit m_editdatalog;
    CStringW m_strip, m_strport, m_stripremote, m_strportremote, m_strportremoteudp, m_strdatalog;
    LRESULT OnBnClickedButtonStart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};
