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
                            wxDefaultPosition, wxSize(800, 450)) {
    wxInitAllImageHandlers();
    wxToolTip::Enable(true);
    wxToolTip::SetAutoPop(500);
    Centre();
    wxWindow::SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_EXTRALIGHT, false, "Courier New"));
    auto *mainSizer = new wxBoxSizer(wxHORIZONTAL);
    SetSizer(mainSizer);
    auto *leftSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(leftSizer, 1, wxEXPAND);
    mainSizer->AddSpacer(8);
    auto *rightSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(rightSizer, 1, wxEXPAND);

    auto *panelSizer = new wxBoxSizer(wxHORIZONTAL);
    leftSizer->Add(panelSizer, 0, wxEXPAND);

    auto *btn = new wxButton(this, wxID_ANY, "&Load", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->Bind(wxEVT_BUTTON, [mainSizer, this](const wxCommandEvent &) {
        wxFileName filename(wxStandardPaths::Get().GetUserConfigDir(), "");
        filename.AppendDir("EldenRing");
        wxFileDialog fileDlg(this, "Select save file to open...", filename.GetAbsolutePath(), wxEmptyString,
                             "All supported formats (*.sl2;*.co2;memory.dat)|*.sl2;*.co2;memory.dat|Steam Save File (*.sl2)|*.sl2|Seamless Co-op Save File (*.co2)|*.co2|PS4 Save File (memory.dat)|memory.dat",
                             wxFD_OPEN | wxFD_SHOW_HIDDEN | wxFD_FILE_MUST_EXIST);
        if (fileDlg.ShowModal() == wxID_CANCEL) return;
        auto *save = new(std::nothrow) SaveFile(fileDlg.GetPath().ToStdWstring());
        if (!save || !save->ok()) {
            delete save;
            wxMessageBox("Invalid save file!", "Error");
            return;
        }
        delete savefile_;
        savefile_ = save;
        filenameText_->SetLabel(fileDlg.GetPath());
        uint64_t userid = 0;
        if (save->saveType() == SaveFile::Steam) {
            save->listSlots(-1, [&userid](int, const SaveSlot &slot) {
                if (slot.slotType != SaveSlot::Summary) return;
                userid = dynamic_cast<const SummarySlot&>(slot).userid;
            });
        }
        saveTypeText_->SetLabel(save->saveType() == SaveFile::Steam ? wxString::Format("Steam Save | UserID: %llu", userid) : wxString("PS4 Save"));
        mainSizer->Layout();
        updateList();
    });
    panelSizer->Add(btn, 0, wxEXPAND);

    btn = new wxButton(this, wxID_ANY, "&Export", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->Disable();
    btn->Bind(wxEVT_BUTTON, [&](const wxCommandEvent &) {
        auto index = charList_->GetFirstSelected();
        if (index < 0) return;
        if (faceMode_) {
            wxFileDialog fileDlg(this, "Export to face file...", wxEmptyString, wxEmptyString,
                                 wxFileSelectorDefaultWildcardStr, wxFD_SAVE | wxFD_SHOW_HIDDEN | wxFD_OVERWRITE_PROMPT);
            if (fileDlg.ShowModal() == wxID_CANCEL) return;
            std::vector<int> slots;
            slots.emplace_back(static_cast<int>(charList_->GetItemData(index)));
            while ((index = charList_->GetNextSelected(index)) >= 0) {
                slots.emplace_back(static_cast<int>(charList_->GetItemData(index)));
            }
            if (!savefile_->exportFacesToFile(fileDlg.GetPath().ToStdWstring(), slots)) {
                wxMessageBox("Failed to export face data!", "Error");
            }
            return;
        }
        wxFileDialog fileDlg(this, "Export to character file...", wxEmptyString, wxEmptyString,
                             wxFileSelectorDefaultWildcardStr, wxFD_SAVE | wxFD_SHOW_HIDDEN | wxFD_OVERWRITE_PROMPT);
        if (fileDlg.ShowModal() == wxID_CANCEL) return;
        if (const auto slot = charList_->GetItemData(index); !savefile_->exportToFile(fileDlg.GetPath().ToStdWstring(), slot)) {
            wxMessageBox("Failed to export character!", "Error");
        }
    });
    panelSizer->Add(btn, 0, wxEXPAND);
    btnExport_ = btn;

    btn = new wxButton(this, wxID_ANY, "&Import", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->Disable();
    btn->Bind(wxEVT_BUTTON, [&](const wxCommandEvent &) {
        const auto index = charList_->GetFirstSelected();
        if (index < 0) return;
        if (faceMode_) {
            wxFileDialog fileDlg(this, "Import from face file...", wxEmptyString, wxEmptyString,
                                 wxFileSelectorDefaultWildcardStr, wxFD_OPEN | wxFD_SHOW_HIDDEN | wxFD_FILE_MUST_EXIST);
            if (fileDlg.ShowModal() == wxID_CANCEL) return;
            auto res = wxMessageBox("Append face(s) data? Choose `No` to overwrite from first selected face slot", "Append or Overwrite", wxYES_NO | wxCANCEL);
            if (res == wxCANCEL) return;
            int slot = -1;
            if (res != wxYES) {
                slot = static_cast<int>(charList_->GetItemData(index));
                if (slot < 0) slot = 0;
            }
            if (savefile_->importFacesFromFile(fileDlg.GetPath().ToStdWstring(), slot)) {
                updateList();
            }
            return;
        }
        wxFileDialog fileDlg(this, "Import from character file...", wxEmptyString, wxEmptyString,
                             wxFileSelectorDefaultWildcardStr, wxFD_OPEN | wxFD_SHOW_HIDDEN | wxFD_FILE_MUST_EXIST);
        if (fileDlg.ShowModal() == wxID_CANCEL) return;
        if (const auto slot = charList_->GetItemData(index); savefile_->importFromFile(fileDlg.GetPath().ToStdWstring(), slot)) {
            charList_->SetItem(index, 1, savefile_->charSlot(slot)->charname);
            charList_->SetItem(index, 2, wxString::Format("%d", savefile_->charSlot(slot)->level));
            updateInfo(index);
        }
    });
    panelSizer->Add(btn, 0, wxEXPAND);
    btnImport_ = btn;

    btn = new wxButton(this, wxID_ANY, "&Switch Mode", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &) {
        faceMode_ = !faceMode_;
        updateInfo(-1);
        updateList();
        const bool enabled = charList_->GetFirstSelected() >= 0;
        btnExport_->Enable(enabled);
        btnImport_->Enable(faceMode_ || enabled);
    });
    panelSizer->Add(btn, 0, wxEXPAND);
    btnSwitch_ = btn;

    rightSizer->Add(new wxStaticText(this, wxID_ANY, "Save File Location:"), 0, wxEXPAND);

    auto *txt = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_START);
    rightSizer->Add(txt, 0, wxEXPAND);
    filenameText_ = txt;

    rightSizer->AddSpacer(10);
    txt = new wxStaticText(this, wxID_ANY, "");
    rightSizer->Add(txt, 0, wxEXPAND);
    saveTypeText_ = txt;

    rightSizer->AddSpacer(10);
    txt = new wxStaticText(this, wxID_ANY, "");
    rightSizer->Add(txt, 1, wxEXPAND);
    charInfoText_ = txt;

    charList_ = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    leftSizer->Add(charList_, 1, wxEXPAND);
    charList_->InsertColumn(0, "Slot");
    charList_->InsertColumn(1, "Name");
    charList_->InsertColumn(2, "Level");
    charList_->SetColumnWidth(0, 50);
    charList_->SetColumnWidth(1, 200);
    charList_->SetColumnWidth(2, 72);
    charList_->Bind(wxEVT_LIST_ITEM_SELECTED, [&](const wxListEvent &ev) {
        const bool enabled = ev.GetIndex() >= 0;
        btnExport_->Enable(enabled);
        btnImport_->Enable(faceMode_ || enabled);
        updateInfo(static_cast<int>(ev.GetData()));
    });
    charList_->Bind(wxEVT_LIST_ITEM_DESELECTED, [&](const wxListEvent &ev) {
        btnExport_->Disable();
        btnImport_->Disable();
        updateInfo(-1);
    });
    updateInfo(-1);
}

MainWnd::~MainWnd() {
    delete savefile_;
    savefile_ = nullptr;
}

void MainWnd::updateList() const {
    if (faceMode_) {
        charList_->DeleteAllItems();
        charList_->DeleteAllColumns();
        charList_->InsertColumn(0, "Slot");
        charList_->InsertColumn(1, "Body Type");
        charList_->SetColumnWidth(0, 50);
        charList_->SetColumnWidth(1, 150);
        charList_->SetSingleStyle(wxLC_SINGLE_SEL, false);
    if (savefile_ == nullptr) return;
        savefile_->listFaces([this](const int slotIndex, const FaceData &face) {
            if (!face.available()) return;
            const auto idx = charList_->InsertItem(slotIndex, wxString::Format("%d", slotIndex + 1));
            charList_->SetItem(idx, 1, face.gender() ? "B" : "A");
            charList_->SetItemData(idx, slotIndex);
        });
        return;
    }
    charList_->DeleteAllItems();
    charList_->DeleteAllColumns();
    charList_->InsertColumn(0, "Slot");
    charList_->InsertColumn(1, "Name");
    charList_->InsertColumn(2, "Level");
    charList_->SetColumnWidth(0, 50);
    charList_->SetColumnWidth(1, 200);
    charList_->SetColumnWidth(2, 72);
    charList_->SetSingleStyle(wxLC_SINGLE_SEL, true);
    if (savefile_ == nullptr) return;
    savefile_->listSlots(-1, [this](const int slotIndex, const SaveSlot &slot) {
        if (slot.slotType != SaveSlot::Character) return;
        const auto &slot2 = dynamic_cast<const CharSlot&>(slot);
        if (slot2.charname.empty()) return;
        const auto idx = charList_->InsertItem(slotIndex, wxString::Format("%d", slotIndex + 1));
        if (slot2.available) {
            charList_->SetItem(idx, 1, slot2.charname);
        } else {
            charList_->SetItem(idx, 1, slot2.charname + L" [Deleted]");
        }
        charList_->SetItem(idx, 2, wxString::Format("%d", slot2.level));
        charList_->SetItemData(idx, slotIndex);
    });
}

void MainWnd::updateInfo(const int index) const {
    if (faceMode_) {
        charInfoText_->SetLabel("Face Mode");
        return;
    }
    if (index < 0) {
        charInfoText_->SetLabel("Character Mode");
        return;
    }
    const auto &slot = dynamic_cast<const CharSlot&>(savefile_->slot(index));
    charInfoText_->SetLabel(wxString::Format(
        "Character Mode\n\n"
        "Name:  %s\n"
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
}
