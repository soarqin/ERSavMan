#include "savefile.h"

#include "mainwnd.h"

class ERSavManApp : public wxApp {
public:
    bool OnInit() override {
        (new MainWnd)->Show(true);
        return true;
    }
};

IMPLEMENT_APP(ERSavManApp)
