// Copyright (c) 2010-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/interface_ui.h>

#include <util/translation.h>

#include <btcsignals.h>

CClientUIInterface uiInterface;

struct UISignals {
    CClientUIInterface::ThreadSafeMessageBoxSigType ThreadSafeMessageBox;
    CClientUIInterface::ThreadSafeQuestionSigType ThreadSafeQuestion;
    CClientUIInterface::InitMessageSigType InitMessage;
    CClientUIInterface::InitWalletSigType InitWallet;
    CClientUIInterface::NotifyNumConnectionsChangedSigType NotifyNumConnectionsChanged;
    CClientUIInterface::NotifyNetworkActiveChangedSigType NotifyNetworkActiveChanged;
    CClientUIInterface::NotifyAlertChangedSigType NotifyAlertChanged;
    CClientUIInterface::ShowProgressSigType ShowProgress;
    CClientUIInterface::NotifyBlockTipSigType NotifyBlockTip;
    CClientUIInterface::NotifyHeaderTipSigType NotifyHeaderTip;
    CClientUIInterface::BannedListChangedSigType BannedListChanged;
};
static UISignals g_ui_signals;

#define ADD_SIGNALS_IMPL_WRAPPER(signal_name)                                                                 \
    btcsignals::connection CClientUIInterface::signal_name##_connect(signal_name##SigType::functype fn) \
    {                                                                                                         \
        return g_ui_signals.signal_name.connect(fn);                                                          \
    }

ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeMessageBox);
ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeQuestion);
ADD_SIGNALS_IMPL_WRAPPER(InitMessage);
ADD_SIGNALS_IMPL_WRAPPER(InitWallet);
ADD_SIGNALS_IMPL_WRAPPER(NotifyNumConnectionsChanged);
ADD_SIGNALS_IMPL_WRAPPER(NotifyNetworkActiveChanged);
ADD_SIGNALS_IMPL_WRAPPER(NotifyAlertChanged);
ADD_SIGNALS_IMPL_WRAPPER(ShowProgress);
ADD_SIGNALS_IMPL_WRAPPER(NotifyBlockTip);
ADD_SIGNALS_IMPL_WRAPPER(NotifyHeaderTip);
ADD_SIGNALS_IMPL_WRAPPER(BannedListChanged);

bool CClientUIInterface::ThreadSafeMessageBox(const bilingual_str& message, const std::string& caption, unsigned int style) { return g_ui_signals.ThreadSafeMessageBox(message, caption, style).value_or(false);}
bool CClientUIInterface::ThreadSafeQuestion(const bilingual_str& message, const std::string& non_interactive_message, const std::string& caption, unsigned int style) { return g_ui_signals.ThreadSafeQuestion(message, non_interactive_message, caption, style).value_or(false);}
void CClientUIInterface::InitMessage(const std::string& message) { g_ui_signals.InitMessage(message); }
void CClientUIInterface::InitWallet() { g_ui_signals.InitWallet(); }
void CClientUIInterface::NotifyNumConnectionsChanged(int newNumConnections) { g_ui_signals.NotifyNumConnectionsChanged(newNumConnections); }
void CClientUIInterface::NotifyNetworkActiveChanged(bool networkActive) { g_ui_signals.NotifyNetworkActiveChanged(networkActive); }
void CClientUIInterface::NotifyAlertChanged() { g_ui_signals.NotifyAlertChanged(); }
void CClientUIInterface::ShowProgress(const std::string& title, int nProgress, bool resume_possible) { g_ui_signals.ShowProgress(title, nProgress, resume_possible); }
void CClientUIInterface::NotifyBlockTip(SynchronizationState s, const CBlockIndex* i) { g_ui_signals.NotifyBlockTip(s, i); }
void CClientUIInterface::NotifyHeaderTip(SynchronizationState s, int64_t height, int64_t timestamp, bool presync) { g_ui_signals.NotifyHeaderTip(s, height, timestamp, presync); }
void CClientUIInterface::BannedListChanged() { g_ui_signals.BannedListChanged(); }

bool InitError(const bilingual_str& str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_ERROR);
    return false;
}

void InitWarning(const bilingual_str& str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING);
}
