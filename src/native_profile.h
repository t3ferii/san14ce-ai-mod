#pragma once
constexpr unsigned kTextSize = 20179374u;
constexpr unsigned kTextCrc32 = 0x6C7F386Eu;
constexpr unsigned kImageTimestamp = 0x6A983C25u;
constexpr unsigned char kCutCallBytes[5] = {0xE8,0xF5,0xE7,0xFE,0xFF};
constexpr unsigned char kPairCallBytes[5] = {0xE8,0xE7,0,0,0};
constexpr unsigned char kDamageEntryBytes[5] = {0x40,0x55,0x57,0x41,0x56}; // three complete PUSH instructions
constexpr unsigned char kAdjacencyEntryBytes[5] = {0x40,0x53,0x55,0x41,0x57};
// One complete position-independent MOV [RSP+10],RBP at 279190.
constexpr unsigned char kBuildPermissionEntryBytes[5] = {0x48,0x89,0x6C,0x24,0x10};
// Complete MOV [RSP+18],R8; the trampoline rejoins before the EDX save.
constexpr unsigned char kDomesticTickEntryBytes[5] = {0x4C,0x89,0x44,0x24,0x18};
constexpr unsigned char kDomesticBudgetBranchBytes[6] = {0x0F,0x8C,0x2B,0x01,0x00,0x00};
constexpr unsigned char kDomesticCostLoadBytes[6] = {0x8B,0x1D,0x61,0xAF,0x85,0x01};
