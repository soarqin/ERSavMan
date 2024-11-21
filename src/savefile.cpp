/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "savefile.h"
#include "mem.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <windows.h>

inline void readWString(std::wstring &output, const uint16_t *input) {
    while (*input) {
        output.push_back(*input++);
    }
}

inline void findUserIDOffset(CharSlot *slot, const std::function<void(int offset)> &func) {
    size_t pointer = 0x1E0000;
    while (true) {
        auto offset = boyer_moore(slot->data.data() + pointer, slot->data.size() - pointer, reinterpret_cast<const uint8_t*>("\x4D\x4F\x45\x47\x00\x26\x04\x21"), 8);
        if (offset == static_cast<size_t>(-1)) return;
        offset += 0x1E0000;
        offset += *reinterpret_cast<int*>(&slot->data[offset - 4]);
        offset += 4;
        if (memcmp(&slot->data[offset], "\x46\x4F\x45\x47\x00\x26\x04\x21", 8) != 0) {
            pointer = offset + 1;
            continue;
        }
        offset += *reinterpret_cast<int*>(&slot->data[offset - 4]);
        offset += *reinterpret_cast<int*>(&slot->data[offset]) + 4 + 0x20078;
        func(static_cast<int>(offset));
        return;
    }
}

inline size_t findStatOffset(const std::vector<uint8_t> &data, const int level, const wchar_t *name) {
    const auto *ptr = data.data();
    const size_t sz = data.size() - 0x130;
    for (size_t i = 0; i < sz; ++i) {
        const auto nl = *reinterpret_cast<const int*>(ptr + i + 0x2C);
        if (nl == level && *reinterpret_cast<const int*>(ptr + i) + *reinterpret_cast<const int*>(ptr + i + 4) + *reinterpret_cast<const int*>(ptr + i + 8) + *reinterpret_cast<const int*>(ptr + i + 12) +
            *reinterpret_cast<const int*>(ptr + i + 16) + *reinterpret_cast<const int*>(ptr + i + 20) + *reinterpret_cast<const int*>(ptr + i + 24) + *reinterpret_cast<const int*>(ptr + i + 28)
            == 79 + nl && lstrcmpW(name, reinterpret_cast<const wchar_t*>(ptr + i + 0x60)) == 0) {
            return i;
        }
    }
    return 0;
}

SaveFile::SaveFile(const std::wstring &filename) {
    std::ifstream stream(filename.c_str(), std::ios::in | std::ios::binary);
    if (!stream.is_open()) return;
    load(stream);
    stream.close();
    if (ok_) filename_ = filename;
}

SaveFile::SaveFile(const std::string &data, const std::wstring &saveAs) {
    std::istringstream stream(data, std::ios::in | std::ios::binary);
    load(stream);
    std::ofstream ofs(saveAs.c_str(), std::ios::out | std::ios::binary);
    if (ofs.is_open()) {
        ofs.write((const char*)data.data(), static_cast<int>(data.size()));
        ofs.close();
    }
    if (ok_) filename_ = saveAs;
}

void SaveFile::load(std::istream &stream) {
    ok_ = false;
    header_.clear();
    slots_.clear();
    faces_.clear();
    summarySlot_ = -1;
    uint32_t magic = 0;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));
    if (magic == 0x34444E42) {
        saveType_ = Steam;
        ok_ = true;
        stream.seekg(0, std::ios::beg);
        header_.resize(0x300);
        stream.read(reinterpret_cast<char*>(header_.data()), 0x300);
        const int slotCount = *reinterpret_cast<int*>(&header_[12]);
        slots_.resize(slotCount);
        for (int i = 0; i < slotCount; ++i) {
            int slotSize = *reinterpret_cast<int*>(&header_[0x48 + i * 0x20]);
            const int slotOffset = *reinterpret_cast<int*>(&header_[0x50 + i * 0x20]);
            const int nameOffset = *reinterpret_cast<int*>(&header_[0x54 + i * 0x20]);
            std::wstring slotFilename;
            readWString(slotFilename, reinterpret_cast<const uint16_t*>(&header_[nameOffset]));
            std::unique_ptr<SaveSlot> slot;
            if (slotSize == 0x280010) {
                slot = std::make_unique<CharSlot>();
                slot->slotType = SaveSlot::Character;
            } else if (slotSize == 0x60010){
                slot = std::make_unique<SummarySlot>();
                slot->slotType = SaveSlot::Summary;
                summarySlot_ = i;
            } else {
                slot = std::make_unique<SaveSlot>();
                slot->slotType = SaveSlot::Other;
            }
            slot->offset = slotOffset;
            slot->filename = slotFilename;
            stream.seekg(slotOffset, std::ios::beg);
            stream.read(reinterpret_cast<char*>(slot->md5hash), 16);
            slotSize -= 16;
            slot->data.resize(slotSize);
            stream.read(reinterpret_cast<char*>(slot->data.data()), slotSize);
            slots_[i] = std::move(slot);
        }
    } else if (magic == 0x2C9C01CB) {
        saveType_ = PS4;
        ok_ = true;
        stream.seekg(0, std::ios::beg);
        header_.resize(0x70);
        stream.read(reinterpret_cast<char*>(header_.data()), 0x70);
        slots_.resize(12);
        for (int i = 0; i < 12; ++i) {
            std::unique_ptr<SaveSlot> slot;
            int slotSize;
            if (i < 10) {
                slot = std::make_unique<CharSlot>();
                slot->slotType = SaveSlot::Character;
                slotSize = 0x280000;
            } else if (i < 11){
                slot = std::make_unique<SummarySlot>();
                slot->slotType = SaveSlot::Summary;
                slotSize = 0x60000;
                summarySlot_ = i;
            } else {
                slot = std::make_unique<SaveSlot>();
                slot->slotType = SaveSlot::Other;
                slotSize = 0x240010;
            }
            slot->offset = static_cast<uint32_t>(stream.tellg());
            slot->data.resize(slotSize);
            stream.read(reinterpret_cast<char*>(slot->data.data()), slotSize);
            slots_[i] = std::move(slot);
        }
    } else {
        return;
    }
    for (size_t idx = 0; idx < slots_.size(); idx++) {
        auto &slot = slots_[idx];
        switch (slot->slotType) {
            case SaveSlot::Character: {
                auto *slot2 = dynamic_cast<CharSlot*>(slot.get());
                slot2->useridOffset = 0;
                slot2->userid = 0;
                findUserIDOffset(slot2, [slot2](const int offset) {
                    slot2->useridOffset = offset;
                    slot2->userid = *reinterpret_cast<uint64_t*>(&slot2->data[offset]);
                });
                auto &summaryData = slots_[summarySlot_]->data;
                const auto rlevel = *reinterpret_cast<int*>(&summaryData[0x195E + 0x24C * idx + 0x22]);
                const auto *rname = reinterpret_cast<const wchar_t*>(&summaryData[0x195E + 0x24C * idx]);
                const auto offset = findStatOffset(slot2->data, rlevel, rname);
                if (offset > 0) {
                    slot2->statOffset = static_cast<int>(offset);
                    slot2->level = *reinterpret_cast<uint16_t*>(slot2->data.data() + offset + 0x2C);
                    memcpy(slot2->stats, slot2->data.data() + offset, 8 * sizeof(uint32_t));
                } else {
                    slot2->statOffset = 0;
                    slot2->level = 0;
                    memset(slot2->stats, 0, sizeof(slot2->stats));
                }
                break;
            }
            case SaveSlot::Summary: {
                auto *slot2 = dynamic_cast<SummarySlot*>(slot.get());
                slot2->userid = *reinterpret_cast<uint64_t*>(&slot2->data[4]);
                if (const auto faceLen = *reinterpret_cast<uint32_t*>(&slot2->data[0x158]); faceLen == 0x11D0) {
                    for (int i = 0; i < 15; i++) {
                        auto faceData = std::make_unique<FaceData>();
                        memcpy(faceData->data, &slot2->data[0x15C + 0x130 * i], 0x130);
                        faces_.emplace_back(std::move(faceData));
                    }
                }
                for (size_t i = 0; i < slots_.size(); ++i) {
                    if (slots_[i]->slotType == SaveSlot::Character) {
                        const auto level = *reinterpret_cast<uint16_t*>(&slot2->data[0x195E + 0x24C * i + 0x22]);
                        if (level == 0) continue;
                        const uint8_t available = *(uint8_t*)&slot2->data[0x1954 + i];
                        auto *slot3 = dynamic_cast<CharSlot*>(slots_[i].get());
                        readWString(slot3->charname, reinterpret_cast<const uint16_t*>(&slot2->data[0x195E + 0x24C * i]));
                        slot3->level = level;
                        slot3->available = available;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

void SaveFile::resign(const uint64_t userid) {
    std::fstream fs(filename_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!fs.is_open()) {
        return;
    }
    for (auto &slot: slots_) {
        auto &data = slot->data;
        if (slot->slotType == SaveSlot::Character) {
            auto *slot2 = dynamic_cast<CharSlot*>(slot.get());
            slot2->userid = userid;
            *reinterpret_cast<uint64_t*>(&data[slot2->useridOffset]) = userid;
        } else if (slot->slotType == SaveSlot::Summary) {
            auto *slot2 = dynamic_cast<SummarySlot*>(slot.get());
            slot2->userid = userid;
            *reinterpret_cast<uint64_t*>(&data[4]) = userid;
        }
        fs.seekg(slot->offset, std::ios::beg);
        md5Hash(data.data(), data.size(), slot->md5hash);
        if (saveType_ == Steam)
            fs.write(reinterpret_cast<const char*>(slot->md5hash), 16);
        fs.write(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
    }
    fs.close();
}

bool SaveFile::resign(const int slot, const uint64_t userid) const {
    if (slot < 0 || slot >= slots_.size() || slots_[slot]->slotType != SaveSlot::Character) { return false; }
    std::fstream fs(filename_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!fs.is_open()) {
        return false;
    }
    {
        auto *slot2 = dynamic_cast<CharSlot*>(slots_[slot].get());
        auto &data = slot2->data;
        slot2->userid = userid;
        *reinterpret_cast<uint64_t*>(&data[slot2->useridOffset]) = userid;
        fs.seekg(slot2->offset, std::ios::beg);
        md5Hash(data.data(), data.size(), slot2->md5hash);
        if (saveType_ == Steam)
            fs.write(reinterpret_cast<const char*>(slot2->md5hash), 16);
        fs.write(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
    }
    {
        auto *slot2 = dynamic_cast<SummarySlot*>(slots_[summarySlot_].get());
        auto &data = slot2->data;
        slot2->userid = userid;
        *reinterpret_cast<uint64_t*>(&data[4]) = userid;
        fs.seekg(slot2->offset, std::ios::beg);
        md5Hash(data.data(), data.size(), slot2->md5hash);
        if (saveType_ == Steam)
            fs.write(reinterpret_cast<const char*>(slot2->md5hash), 16);
        fs.write(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
    }
    fs.close();
    return true;
}

void SaveFile::patchSlotTime(const int slot, const uint32_t millisec) const {
    {
        auto &s = slots_[slot];
        if (s->slotType != SaveSlot::Character) { return; }
        *reinterpret_cast<uint32_t*>(s->data.data() + 8) = millisec;
    }
    {
        auto &s = slots_[summarySlot_];
        *reinterpret_cast<uint16_t*>(&s->data[0x195E + 0x24C * slot + 0x22 + 0x04]) = millisec / 1000;
    }
}

bool SaveFile::exportTo(std::vector<uint8_t> &buf, const int slot) const {
    if (slot < 0 || slot >= slots_.size() || slots_[slot]->slotType != SaveSlot::Character) { return false; }
    const auto &data = slots_[slot]->data;
    buf.resize(data.size() + 0x24C);
    memcpy(buf.data(), data.data(), data.size());
    const auto &summary = slots_[summarySlot_]->data;
    memcpy(buf.data() + data.size(), summary.data() + 0x195E + 0x24C * slot, 0x24C);
    return true;
}

bool SaveFile::exportToFile(const std::wstring &filename, const int slot) const {
    std::vector<uint8_t> buf;
    if (!exportTo(buf, slot)) return false;
    std::ofstream ofs(filename.c_str(), std::ios::out | std::ios::binary);
    if (!ofs.is_open()) { return false; }
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()));
    ofs.close();
    return true;
}

bool SaveFile::importFrom(const std::vector<uint8_t> &buf, const int slot, const std::function<void()>& beforeResign, const bool keepFace) const {
    if (slot < 0 || slot >= slots_.size() || slots_[slot]->slotType != SaveSlot::Character) { return false; }
    auto *slot2 = dynamic_cast<CharSlot*>(slots_[slot].get());
    auto &data = slot2->data;
    if (buf.size() != data.size() + 0x24C) { return false; }

    std::vector<uint8_t> face;
    if (keepFace) {
        if (!exportFace(face, slot)) face.clear();
    }
    memcpy(data.data(), buf.data(), data.size());

    slot2->useridOffset = 0;
    slot2->userid = 0;
    findUserIDOffset(slot2, [slot2](const int offset) {
        slot2->useridOffset = offset;
        slot2->userid = *reinterpret_cast<uint64_t*>(&slot2->data[offset]);
    });

    auto *ndata = buf.data() + data.size();
    const auto level = *reinterpret_cast<const int*>(ndata + 0x22);
    const auto *name = reinterpret_cast<const wchar_t*>(ndata);
    const auto offset = findStatOffset(data, level, name);
    if (offset > 0) {
        slot2->statOffset = static_cast<int>(offset);
        slot2->level = *reinterpret_cast<uint16_t*>(slot2->data.data() + offset + 0x2C);
        memcpy(slot2->stats, slot2->data.data() + offset, 8 * sizeof(uint32_t));
    } else {
        slot2->statOffset = 0;
        slot2->level = 0;
        memset(slot2->stats, 0, sizeof(slot2->stats));
    }

    auto *summary = dynamic_cast<SummarySlot*>(slots_[summarySlot_].get());
    summary->data[0x1954 + slot] = 1;
    summary->data[0x1334] = static_cast<int8_t>(slot);
    memcpy(&summary->data[0x195E + 0x24C * slot], ndata, 0x24C);
    if (keepFace && !face.empty()) {
        return importCharFace(face, slot, false);
    }
    if (beforeResign) {
        beforeResign();
    }
    return resign(slot, summary->userid);
}

bool SaveFile::importFromFile(const std::wstring &filename, const int slot, const std::function<void()>& beforeResign, const bool keepFace) const {
    if (slot < 0 || slot >= slots_.size() || slots_[slot]->slotType != SaveSlot::Character) { return false; }
    std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::binary);
    if (!ifs.is_open()) { return false; }
    ifs.seekg(0, std::ios::end);
    const auto &data = slots_[slot]->data;
    if (ifs.tellg() != data.size() + 0x24C) { ifs.close(); return false; }
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(data.size() + 0x24C);
    ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<int>(data.size()) + 0x24C);
    ifs.close();
    return importFrom(buf, slot, beforeResign, keepFace);
}

bool SaveFile::exportFace(std::vector<uint8_t> &buf, const int slot) const {
    if (slot < 0 || slot >= slots_.size() || slots_[slot]->slotType != SaveSlot::Character) { return false; }
    const auto &data = slots_[slot]->data;
    const auto *m = data.data();
    const auto offset = boyer_moore(m, data.size(), reinterpret_cast<const uint8_t*>("\x46\x41\x43\x45\x04\x00\x00\x00\x20\x01\x00\x00"), 12);
    buf.assign(m + offset, m + offset + 0x120);
    const auto *s = dynamic_cast<const CharSlot*>(slots_[slot].get());
    if (s->statOffset > 0)
        buf.push_back(data[s->statOffset + 0x82]);
    else
        buf.push_back(255);
    return true;
}

bool SaveFile::exportCharFaceToFile(const std::wstring &filename, const int slot) const {
    std::vector<uint8_t> buf;
    if (!exportFace(buf, slot)) { return false; }
    std::ofstream ofs(filename.c_str(), std::ios::out | std::ios::binary);
    if (!ofs.is_open()) { return false; }
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()));
    ofs.close();
    return true;
}

bool SaveFile::importCharFace(const std::vector<uint8_t> &buf, const int slot, const bool resign) const {
    if (slot < 0 || slot >= slots_.size() || slots_[slot]->slotType != SaveSlot::Character) { return false; }
    if (memcmp(buf.data(), "\x46\x41\x43\x45\x04\x00\x00\x00\x20\x01\x00\x00", 12) != 0) { return false; }
    const auto sex = buf[0x120];
    if (sex == 0xFF) { return true; }
    auto *slot2 = dynamic_cast<CharSlot*>(slots_[slot].get());
    if (slot2->statOffset == 0) { return false; }
    auto &data = slot2->data;
    auto *m = data.data();
    const auto offset = boyer_moore(m, data.size(), reinterpret_cast<const uint8_t*>("\x46\x41\x43\x45\x04\x00\x00\x00\x20\x01\x00\x00"), 12);
    memcpy(m + offset, buf.data(), 0x120);
    slot2->data[slot2->statOffset + 0x82] = sex;
    auto &s = slots_[summarySlot_];
    memcpy(&s->data[0x195E + 0x24C * slot + 0x22 + 0x18], buf.data(), 0x120);
    s->data[0x195E + 0x24C * slot + 0x242] = sex;
    if (resign) {
        std::fstream fs(filename_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
        if (!fs.is_open()) return false;
        md5Hash(s->data.data(), s->data.size(), s->md5hash);
        fs.seekg(s->offset, std::ios::beg);
        if (saveType_ == Steam)
            fs.write(reinterpret_cast<const char*>(s->md5hash), 16);
        fs.write(reinterpret_cast<const char*>(s->data.data()), static_cast<int>(s->data.size()));
        fs.seekg(slot2->offset, std::ios::beg);
        md5Hash(data.data(), data.size(), slot2->md5hash);
        if (saveType_ == Steam)
            fs.write(reinterpret_cast<const char*>(slot2->md5hash), 16);
        fs.write(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
        fs.close();
    }
    return true;
}

bool SaveFile::importCharFaceFromFile(const std::wstring &filename, const int slot, const bool resign) const {
    std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::binary);
    if (!ifs.is_open()) { return false; }
    ifs.seekg(0, std::ios::end);
    if (ifs.tellg() != 0x120) { ifs.close(); return false; }
    std::vector<uint8_t> face(0x120);
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(face.data()), 0x120);
    ifs.close();

    return importCharFace(face, slot, resign);
}

bool SaveFile::exportFacesToFile(const std::wstring &filename, const std::vector<int> &slot) const {
    if (slot.empty()) return false;
    std::ofstream ofs(filename.c_str(), std::ios::out | std::ios::binary);
    if (!ofs.is_open()) return false;
    for (const auto &s: slot) {
        const auto &f = faces_[s];
        ofs.write(reinterpret_cast<const char*>(f->data), 0x130);
    }
    ofs.close();
    return true;
}

bool SaveFile::importFacesFromFile(const std::wstring &filename, int slot) {
    bool findEmptySlot = slot < 0;
    std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::binary);
    if (!ifs.is_open()) return false;
    std::fstream fs(filename_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!fs.is_open()) return false;
    auto *slot2 = dynamic_cast<SummarySlot*>(slots_[summarySlot_].get());
    auto &data = slot2->data;

    ifs.seekg(0, std::ios::end);
    const auto sz = static_cast<int>(ifs.tellg());
    if (sz % 0x130 != 0) { ifs.close(); return false; }
    const auto count = sz / 0x130;
    ifs.seekg(0, std::ios::beg);

    for (int i = 0; i < count; ++i) {
        auto faceData = std::make_unique<FaceData>();
        ifs.read(reinterpret_cast<char*>(faceData->data), 0x130);
        if (findEmptySlot) {
            for (int j = 0; j < faces_.size(); ++j) {
                if (!faces_[j]->available()) {
                    slot = j;
                    break;
                }
            }
            if (slot < 0 || slot >= faces_.size())
                break;
        } else {
            if (slot >= faces_.size())
                break;
        }
        faces_[slot] = std::move(faceData);
        memcpy(&data[0x15C + slot * 0x130], faces_[slot]->data, 0x130);

        if (!findEmptySlot) {
            slot++;
        }
    }

    fs.seekg(slot2->offset, std::ios::beg);
    md5Hash(data.data(), data.size(), slot2->md5hash);
    if (saveType_ == Steam)
        fs.write(reinterpret_cast<const char*>(slot2->md5hash), 16);
    fs.write(reinterpret_cast<const char*>(data.data()), 0x11D0 + 0x158 + 4);

    fs.close();
    ifs.close();
    return true;
}

void SaveFile::listSlots(int slot, const std::function<void(int, const SaveSlot&)> &func) const {
#if defined(USE_CLI)
    fprintf(stdout, "SaveType: %s\n", saveType_ == Steam ? "Steam" : "PS4");
    if (saveType_ == Steam) {
        auto &s = slots_[summarySlot_];
        fprintf(stdout, "User ID:  %llu\n", ((SummarySlot *)s.get())->userid);
    }
    if (slot < 0) {
        for (int i = 0; i < slots_.size(); ++i) {
            listSlot(i);
        }
    } else if (slot < slots_.size()) {
        listSlot(slot);
    }
#else
    const auto sz = static_cast<int>(slots_.size());
    if (slot < 0) {
        for (int i = 0; i < sz; ++i) {
            func(i, *slots_[i]);
        }
    } else if (slot < sz) {
        func(slot, *slots_[slot]);
    }
#endif
}

void SaveFile::listFaces(const std::function<void(int, const FaceData &)> &func) const {
    const auto sz = static_cast<int>(faces_.size());
    for (int i = 0; i < sz; ++i) {
        func(i, *faces_[i]);
    }
}

void SaveFile::listSlot(const int slot) const {
    auto &s = slots_[slot];
    switch (s->slotType) {
    case SaveSlot::Character: {
        const auto *slot2 = dynamic_cast<CharSlot*>(s.get());
        if (slot2->charname.empty()) break;
        fprintf(stdout, "%4d: %ls (Level %d)\n", slot, slot2->charname.c_str(), slot2->level);
        break;
    }
    case SaveSlot::Summary:
        fprintf(stdout, "%4d: Summary\n", slot);
        break;
    default:
        fprintf(stdout, "%4d: Other\n", slot);
        break;
    }
}

void SaveFile::fixHashes() {
    if (saveType_ != Steam) { return; }
    std::fstream fs(filename_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    for (const auto &slot: slots_) {
        uint8_t hash[16];
        md5Hash(slot->data.data(), slot->data.size(), hash);
        if (memcmp(hash, slot->md5hash, 16) != 0) {
            memcpy(slot->md5hash, hash, 16);
            fs.seekg(slot->offset, std::ios::beg);
            fs.write(reinterpret_cast<const char*>(hash), 16);
        }
    }
    fs.close();
}

bool SaveFile::verifyHashes() const {
    if (saveType_ != Steam) { return true; }
    for (auto &slot: slots_) {
        uint8_t hash[16];
        md5Hash(slot->data.data(), slot->data.size(), hash);
        if (memcmp(hash, slot->md5hash, 16) != 0) {
            return false;
        }
    }
    return true;
}
