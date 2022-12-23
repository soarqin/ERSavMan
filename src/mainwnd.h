/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/listctrl.h>

class SaveFile;
class MainWnd : public wxFrame {
public:
    MainWnd();
    ~MainWnd() override;

    void updateList();

private:
    wxButton *btnExport_, *btnImport_;
    wxStaticText *filenameText_, *saveTypeText_, *charInfoText_;
    wxListCtrl *charList_;
    SaveFile *savefile_ = nullptr;
};
