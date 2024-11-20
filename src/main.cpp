#include "savefile.h"

#include "mainwnd.h"

class ERSavManApp : public wxApp {
public:
    bool OnInit() override {
#if !defined(NDEBUG)
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif

        MSWEnableDarkMode(wxApp::DarkMode_Auto);
        (new MainWnd)->Show(true);
        return true;
    }
};

IMPLEMENT_APP(ERSavManApp)
