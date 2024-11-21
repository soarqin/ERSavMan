/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <istream>
#include <cstdint>

struct SaveSlot {
    enum SlotType {
        Character,
        Summary,
        Other,
    };
    virtual ~SaveSlot() = default;
    SlotType slotType;
    uint32_t offset = 0;
    std::vector<uint8_t> data;
    uint8_t md5hash[16] = {};
    std::wstring filename;
};

struct CharSlot: SaveSlot {
    std::wstring charname;
    uint16_t level = 0;
    uint32_t stats[8] = {};
    int useridOffset = 0;
    int statOffset = 0;
    uint64_t userid = 0;
    bool available = false;
};

struct SummarySlot: SaveSlot {
    uint64_t userid = 0;
};

struct FaceData {
    uint8_t data[0x130];
    [[nodiscard]] bool available() const { return data[0] != 0; }
    [[nodiscard]] uint8_t gender() const { return data[1]; }
};

class SaveFile {
public:
    enum SaveType {
        Steam,
        PS4,
    };
    explicit SaveFile(const std::wstring &filename);
    explicit SaveFile(const std::string &data, const std::wstring &saveAs);
    void load(std::istream &stream);
    void resign(uint64_t userid);
    [[nodiscard]] bool resign(int slot, uint64_t userid) const;
    void patchSlotTime(int slot, uint32_t millisec) const;
    [[nodiscard]] bool ok() const { return ok_; }
    bool exportTo(std::vector<uint8_t> &buf, int slot) const;
    [[nodiscard]] bool exportToFile(const std::wstring &filename, int slot) const;
    bool importFrom(const std::vector<uint8_t> &buf, int slot, const std::function<void()>& beforeResign = nullptr, bool keepFace = false) const;
    bool importFromFile(const std::wstring &filename, int slot, const std::function<void()>& beforeResign = nullptr, bool keepFace = false) const;
    bool exportFace(std::vector<uint8_t> &buf, int slot) const;
    [[nodiscard]] bool exportCharFaceToFile(const std::wstring &filename, int slot) const;
    [[nodiscard]] bool importCharFace(const std::vector<uint8_t> &buf, int slot, bool resign = true) const;
    [[nodiscard]] bool importCharFaceFromFile(const std::wstring &filename, int slot, bool resign = true) const;
    [[nodiscard]] bool exportFacesToFile(const std::wstring &filename, const std::vector<int> &slot) const;
    [[nodiscard]] bool importFacesFromFile(const std::wstring &filename, int slot = -1);
    void listSlots(int slot = -1, const std::function<void(int, const SaveSlot&)> &func = nullptr) const;
    void listFaces(const std::function<void(int, const FaceData&)> &func) const;
    void fixHashes();
    [[nodiscard]] bool verifyHashes() const;

    [[nodiscard]] SaveType saveType() const { return saveType_; }
    [[nodiscard]] size_t count() const { return slots_.size(); }
    [[nodiscard]] const SaveSlot &slot(const size_t index) const { return *slots_[index]; }
    [[nodiscard]] const CharSlot *charSlot(const int index) const { const auto *s = slots_[index].get(); if (s->slotType == SaveSlot::Character) return dynamic_cast<const CharSlot*>(s); return nullptr; }
    [[nodiscard]] const SummarySlot &summarySlot() const { auto *slot = slots_[summarySlot_].get(); return *dynamic_cast<SummarySlot*>(slot); }
    [[nodiscard]] size_t faceCount() const { return faces_.size(); }
    [[nodiscard]] const FaceData *face(const size_t index) const { return faces_[index].get(); }

private:
    void listSlot(int slot) const;

bool ok_ = false;
    std::wstring filename_;
    SaveType saveType_ = Steam;
    std::vector<uint8_t> header_;
    std::vector<std::unique_ptr<SaveSlot>> slots_;
    int summarySlot_ = -1;

    // up to 15 faces can be retrieved from summary slot
    std::vector<std::unique_ptr<FaceData>> faces_;
};
