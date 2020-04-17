#include "StdAfx.h"
#include "MainDlg.h"

LRESULT CMainDlg::OnBnClickedButtonStart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    // TODO: Add your control notification handler code here
    DoDataExchange(TRUE);

    if (m_proxy.IsRunning())
    {
        m_proxy.SetProxy((char*)CStringA(m_strip).GetString(), 0);
        m_proxy.Uninit();
        Log("stop sucess ...");
        GetDlgItem(IDC_BUTTON_START).SetWindowText(L"开始加速");
    }
    else
    {
        if (0 == m_proxy.SetProxy((char*)CStringA(m_strip).GetString(), _wtoi(m_strport.GetString())) &&
            0 == m_proxy.Init(inet_addr(CStringA(m_strip).GetString()), htons(_wtoi(m_strport.GetString())),
            inet_addr(CStringA(m_stripremote).GetString()), htons(_wtoi(m_strportremote.GetString())), htons(_wtoi(m_strportremoteudp.GetString()))))
        {
            Log("start sucess ...");
            GetDlgItem(IDC_BUTTON_START).SetWindowText(L"停止加速");
        }
        else
        {
            ::MessageBoxW(NULL, L"Error", L"开启失败", 0);
        }
    }
    return 0;
}
