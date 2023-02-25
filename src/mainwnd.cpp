/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "mainwnd.h"
#include "savefile.h"
#include "version.h"
#include <wx/filename.h>
#include <wx/stdpaths.h>

MainWnd::MainWnd(): wxFrame(nullptr, wxID_ANY, wxT("ELDEN RING Save Manager " VERSION_STR),
                            wxDefaultPosition, wxSize(600, 300)) {
    wxInitAllImageHandlers();
    wxToolTip::Enable(true);
    wxToolTip::SetAutoPop(500);
    Centre();
    wxWindow::SetBackgroundColour(wxColour(243, 243, 243));
    wxFont font(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Couriers New");
    wxWindow::SetFont(font);
    auto *mainSizer = new wxBoxSizer(wxHORIZONTAL);
    SetSizer(mainSizer);
    auto *leftSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(leftSizer, wxSizerFlags(1).Expand().Border());
    auto *rightSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(rightSizer, wxSizerFlags(1).Align(wxALIGN_TOP).Border());

    auto *panelSizer = new wxBoxSizer(wxHORIZONTAL);
    leftSizer->Add(panelSizer, wxSizerFlags(0).Expand());

    auto *btn = new wxButton(this, wxID_ANY, "&Load", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->Bind(wxEVT_BUTTON, [rightSizer, this](const wxCommandEvent&) {
        wxFileName filename(wxStandardPaths::Get().GetUserConfigDir(), "");
        filename.AppendDir("EldenRing");
        wxFileDialog fileDlg(this, "Select save file to open...", filename.GetAbsolutePath(), wxEmptyString,
                             "All supported formats (*.sl2;*.co2;memory.dat)|*.sl2;*.co2;memory.dat|Steam Save File (*.sl2)|*.sl2|Seamless Co-op Save File (*.co2)|*.co2|PS4 Save File (memory.dat)|memory.dat",
                             wxFD_OPEN | wxFD_SHOW_HIDDEN | wxFD_FILE_MUST_EXIST);
        if (fileDlg.ShowModal() == wxID_CANCEL) return;
        auto *save = new(std::nothrow) SaveFile(fileDlg.GetPath().utf8_string());
        if (!save || !save->ok()) {
            delete save;
            save = nullptr;
            wxMessageBox("Invalid save file!", "Error");
            return;
        }
        delete savefile_;
        savefile_ = save;
        filenameText_->SetLabel(fileDlg.GetPath());
        uint64_t userid = 0;
        if (save->saveType() == SaveFile::Steam) {
            save->listSlots(-1, [&](int, const SaveSlot &slot) {
                if (slot.slotType != SaveSlot::Summary) return;
                userid = ((SummarySlot&)slot).userid;
            });
        }
        saveTypeText_->SetLabel(save->saveType() == SaveFile::Steam ? wxString::Format("Steam Save | UserID: %llu", userid) : wxString("PS4 Save"));
        rightSizer->Layout();
        updateList();
    });
    panelSizer->Add(btn, wxSizerFlags().Expand());

    btn = new wxButton(this, wxID_ANY, "&Export", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->Disable();
    btn->Bind(wxEVT_BUTTON, [&](const wxCommandEvent&) {
        auto index = charList_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (index < 0) return;
        wxFileDialog fileDlg(this, "Export to character file...", wxEmptyString, wxEmptyString,
                             wxFileSelectorDefaultWildcardStr,wxFD_SAVE | wxFD_SHOW_HIDDEN | wxFD_OVERWRITE_PROMPT);
        if (fileDlg.ShowModal() == wxID_CANCEL) return;
        auto slot = charList_->GetItemData(index);
        savefile_->exportToFile(fileDlg.GetPath().utf8_string(), slot);
    });
    panelSizer->Add(btn, wxSizerFlags().Expand());
    btnExport_ = btn;

    btn = new wxButton(this, wxID_ANY, "&Import", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->Disable();
    btn->Bind(wxEVT_BUTTON, [&](const wxCommandEvent&) {
        auto index = charList_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (index < 0) return;
        wxFileDialog fileDlg(this, "Import from character file...", wxEmptyString, wxEmptyString,
                             wxFileSelectorDefaultWildcardStr,wxFD_OPEN | wxFD_SHOW_HIDDEN | wxFD_FILE_MUST_EXIST);
        if (fileDlg.ShowModal() == wxID_CANCEL) return;
        auto slot = charList_->GetItemData(index);
        if (savefile_->importFromFile(fileDlg.GetPath().utf8_string(), slot)) {
            updateList();
        }
    });
    panelSizer->Add(btn, wxSizerFlags().Expand());
    btnImport_ = btn;

    rightSizer->Add(new wxStaticText(this, wxID_ANY, "Save File Location:"), wxSizerFlags(1).Expand());

    auto *txt = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_START);
    rightSizer->Add(txt, wxSizerFlags().Expand());
    filenameText_ = txt;

    rightSizer->AddSpacer(10);
    txt = new wxStaticText(this, wxID_ANY, "");
    rightSizer->Add(txt, wxSizerFlags().Expand());
    saveTypeText_ = txt;

    rightSizer->AddSpacer(10);
    txt = new wxStaticText(this, wxID_ANY, "");
    rightSizer->Add(txt, wxSizerFlags(1).Expand());
    charInfoText_ = txt;

    charList_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    leftSizer->Add(charList_, wxSizerFlags(1).Expand());
    charList_->InsertColumn(0, "Slot");
    charList_->InsertColumn(1, "Name");
    charList_->InsertColumn(2, "Level");
    charList_->SetColumnWidth(0, 50);
    charList_->SetColumnWidth(1, 200);
    charList_->SetColumnWidth(2, 50);
    charList_->Bind(wxEVT_LIST_ITEM_SELECTED, [&](const wxListEvent &ev) {
        bool enabled = ev.GetIndex() >= 0;
        btnExport_->Enable(enabled);
        btnImport_->Enable(enabled);
        auto &slot = (CharSlot&)savefile_->slot(ev.GetData());
        charInfoText_->SetLabel(wxString::Format("Name:  %s\n"
                                                 "Level: %d\n"
                                                 "Vigor: %u\n"
                                                 "Mind:  %u\n"
                                                 "Endur: %u\n"
                                                 "Str:   %u\n"
                                                 "Dex:   %u\n"
                                                 "Int:   %u\n"
                                                 "Faith: %u\n"
                                                 "Arc:   %u\n",
                                                 slot.charname, slot.level, slot.stats[0], slot.stats[1],
                                                 slot.stats[2], slot.stats[3], slot.stats[4], slot.stats[5],
                                                 slot.stats[6], slot.stats[7]));
    });
    charList_->Bind(wxEVT_LIST_ITEM_DESELECTED, [&](const wxListEvent &ev) {
        btnExport_->Disable();
        btnImport_->Disable();
        charInfoText_->SetLabel("");
    });
}

MainWnd::~MainWnd() {
    delete savefile_;
    savefile_ = nullptr;
}

void MainWnd::updateList() {
    auto index = charList_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    auto slotIdx = index < 0 ? -1 : charList_->GetItemData(index);
    charList_->DeleteAllItems();
    savefile_->listSlots(-1, [&](int index, const SaveSlot &slot){
        if (slot.slotType != SaveSlot::Character) return;
        auto &slot2 = (CharSlot&)slot;
        if (slot2.charname.empty()) return;
        auto idx = charList_->InsertItem(index, wxString::Format("%d", index + 1));
        charList_->SetItem(idx, 1, slot2.charname);
        charList_->SetItem(idx, 2, wxString::Format("%d", slot2.level));
        charList_->SetItemData(idx, index);
        if (index == slotIdx) {
            charList_->SetItemState(idx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        }
    });
}
