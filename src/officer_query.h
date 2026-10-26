#pragma once
// Officer search engine: typed condition tree evaluated over a snapshot.
// Pure C++17; no game calls. Native readers live in native_officer_search.inl.
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace officer_query {
enum class Kind : uint8_t { Number, Label, Set, Text };
enum class Op : uint8_t {
    AtLeast, AtMost, Equal, Between, // Number
    AnyOf,                           // Label: value is one of
    HasAny, HasAll,                  // Set
    Contains,                        // Text
    Known                            // any kind: value is visible / non-empty
};
constexpr int64_t kUnknown = INT64_MIN;

struct Field {
    uint16_t id = 0;       // stable catalog id, saved in presets
    Kind kind = Kind::Number;
    uint16_t slot = 0;     // index inside the Record vector of its kind
    std::wstring name, category;
    int minimum = 0, maximum = 0; // number input bounds
};

// One officer. Labels are interned per field (0 = unknown/hidden).
struct Record {
    uint32_t id = 0;
    std::wstring name;
    std::vector<int64_t> numbers;
    std::vector<uint32_t> labels;
    std::vector<std::vector<uint32_t>> sets; // sorted, unique
};

struct Condition {
    uint16_t field = 0;
    Op op = Op::AtLeast;
    bool negate = false;
    int64_t a = 0, b = 0;
    std::vector<uint32_t> values; // label ids (interned) or set member ids
    std::wstring text;
};
struct Group {
    bool any = false; // false: every child must match (AND); true: OR
    bool negate = false;
    std::vector<Condition> conditions;
    std::vector<Group> groups;
    bool empty() const {
        if (!conditions.empty()) return false;
        for (const auto &g : groups) if (!g.empty()) return false;
        return true;
    }
    size_t size() const {
        size_t n = conditions.size();
        for (const auto &g : groups) n += g.size();
        return n;
    }
};

inline std::wstring folded(std::wstring s) {
    for (auto &c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

// Interned display values per label field. Ids start at 1.
class Labels {
public:
    uint32_t intern(uint16_t field, const std::wstring &text) {
        if (text.empty()) return 0;
        auto &m = ids_[field];
        auto f = m.find(text);
        if (f != m.end()) return f->second;
        auto &names = names_[field];
        names.push_back(text);
        uint32_t id = static_cast<uint32_t>(names.size());
        m.emplace(text, id);
        return id;
    }
    uint32_t find(uint16_t field, const std::wstring &text) const {
        auto m = ids_.find(field);
        if (m == ids_.end()) return 0;
        auto f = m->second.find(text);
        return f == m->second.end() ? 0 : f->second;
    }
    std::wstring text(uint16_t field, uint32_t id) const {
        auto n = names_.find(field);
        if (n == names_.end() || !id || id > n->second.size()) return L"";
        return n->second[id - 1];
    }
    void clear() { ids_.clear(); names_.clear(); }
private:
    std::map<uint16_t, std::map<std::wstring, uint32_t>> ids_;
    std::map<uint16_t, std::vector<std::wstring>> names_;
};

class Catalog {
public:
    const Field &add(uint16_t id, Kind kind, std::wstring name, std::wstring category, int lo = 0, int hi = 0) {
        Field f;
        f.id = id; f.kind = kind; f.name = std::move(name); f.category = std::move(category);
        f.minimum = lo; f.maximum = hi;
        f.slot = counts_[static_cast<int>(kind)]++;
        fields_.push_back(f);
        return fields_.back();
    }
    const Field *find(uint16_t id) const {
        for (const auto &f : fields_) if (f.id == id) return &f;
        return nullptr;
    }
    const std::vector<Field> &fields() const { return fields_; }
    uint16_t count(Kind k) const { return counts_[static_cast<int>(k)]; }
    Record blank() const {
        Record r;
        r.numbers.assign(count(Kind::Number), kUnknown);
        r.labels.assign(count(Kind::Label), 0);
        r.sets.assign(count(Kind::Set), {});
        return r;
    }
private:
    std::vector<Field> fields_;
    uint16_t counts_[4] = {};
};

// Tri-state: 1 true, 0 false, -1 unknown (never counted as a match).
inline int test(const Catalog &cat, const Condition &c, const Record &r) {
    const Field *f = cat.find(c.field);
    if (!f) return -1;
    int result = -1;
    switch (f->kind) {
    case Kind::Number: {
        if (f->slot >= r.numbers.size()) return -1;
        int64_t v = r.numbers[f->slot];
        if (c.op == Op::Known) { result = v != kUnknown; break; }
        if (v == kUnknown) return -1;
        if (c.op == Op::AtLeast) result = v >= c.a;
        else if (c.op == Op::AtMost) result = v <= c.a;
        else if (c.op == Op::Equal) result = v == c.a;
        else if (c.op == Op::Between) result = v >= (std::min)(c.a, c.b) && v <= (std::max)(c.a, c.b);
        break;
    }
    case Kind::Label: {
        if (f->slot >= r.labels.size()) return -1;
        uint32_t v = r.labels[f->slot];
        if (c.op == Op::Known) { result = v != 0; break; }
        if (!v) return -1;
        if (c.op == Op::AnyOf) result = std::find(c.values.begin(), c.values.end(), v) != c.values.end();
        break;
    }
    case Kind::Set: {
        if (f->slot >= r.sets.size()) return -1;
        const auto &s = r.sets[f->slot];
        auto has = [&](uint32_t x) { return std::binary_search(s.begin(), s.end(), x); };
        if (c.op == Op::Known) result = !s.empty();
        else if (c.op == Op::HasAny) result = std::any_of(c.values.begin(), c.values.end(), has);
        else if (c.op == Op::HasAll) result = !c.values.empty() && std::all_of(c.values.begin(), c.values.end(), has);
        break;
    }
    case Kind::Text:
        if (c.op == Op::Contains) result = folded(r.name).find(folded(c.text)) != std::wstring::npos;
        else if (c.op == Op::Known) result = !r.name.empty();
        break;
    }
    if (result < 0) return -1;
    return c.negate ? !result : result;
}

inline int test(const Catalog &cat, const Group &g, const Record &r) {
    if (g.empty()) return 1;
    bool unknown = false;
    auto combine = [&](int v) -> int {
        if (v < 0) { unknown = true; return -2; }
        if (g.any && v) return 1;
        if (!g.any && !v) return 0;
        return -2; // keep scanning
    };
    int decided = -2;
    for (const auto &c : g.conditions) if ((decided = combine(test(cat, c, r))) != -2) break;
    // An empty child group (e.g. just added in the editor) places no constraint.
    if (decided == -2) for (const auto &child : g.groups) {
        if (child.empty()) continue;
        if ((decided = combine(test(cat, child, r))) != -2) break;
    }
    int value;
    if (decided != -2) value = decided;
    else if (unknown) return -1;
    else value = g.any ? 0 : 1;
    return g.negate ? !value : value;
}
inline bool matches(const Catalog &cat, const Group &g, const Record &r) { return test(cat, g, r) == 1; }

inline std::vector<size_t> evaluate(const Catalog &cat, const Group &g, const std::vector<Record> &records) {
    std::vector<size_t> out;
    out.reserve(records.size());
    for (size_t i = 0; i < records.size(); ++i) if (matches(cat, g, records[i])) out.push_back(i);
    return out;
}

// ---- Human-readable summary -------------------------------------------------
using MemberName = std::function<std::wstring(uint16_t field, uint32_t member)>;
inline std::wstring join(const std::vector<std::wstring> &v, const wchar_t *sep, size_t limit = 6) {
    std::wstring s;
    for (size_t i = 0; i < v.size() && i < limit; ++i) { if (i) s += sep; s += v[i]; }
    if (v.size() > limit) s += L" 외 " + std::to_wstring(v.size() - limit);
    return s;
}
inline std::wstring describe(const Catalog &cat, const Condition &c, const MemberName &member) {
    const Field *f = cat.find(c.field);
    if (!f) return L"(알 수 없는 조건)";
    std::wstring s = f->name;
    std::vector<std::wstring> names;
    for (uint32_t v : c.values) names.push_back(member(c.field, v));
    switch (c.op) {
    case Op::AtLeast: s += L" " + std::to_wstring(c.a) + L" 이상"; break;
    case Op::AtMost: s += L" " + std::to_wstring(c.a) + L" 이하"; break;
    case Op::Equal: s += L" = " + std::to_wstring(c.a); break;
    case Op::Between: s += L" " + std::to_wstring((std::min)(c.a, c.b)) + L"~" + std::to_wstring((std::max)(c.a, c.b)); break;
    case Op::AnyOf: s += L": " + join(names, L"·"); break;
    case Op::HasAny: s += L" 중 하나 이상: " + join(names, L"·"); break;
    case Op::HasAll: s += L" 모두 보유: " + join(names, L"·"); break;
    case Op::Contains: s += L"에 \"" + c.text + L"\" 포함"; break;
    case Op::Known: s += f->kind == Kind::Set ? L" 보유" : L" 확인됨"; break;
    }
    return c.negate ? L"아님[" + s + L"]" : s;
}
inline std::wstring describe(const Catalog &cat, const Group &g, const MemberName &member, bool top = true) {
    if (g.empty()) return top ? L"조건 없음 (전체)" : L"( )";
    std::vector<std::wstring> parts;
    for (const auto &c : g.conditions) parts.push_back(describe(cat, c, member));
    for (const auto &child : g.groups) parts.push_back(describe(cat, child, member, false));
    std::wstring s = join(parts, g.any ? L" 또는 " : L" 그리고 ", 64);
    if (!top || g.negate) s = L"(" + s + L")";
    return g.negate ? L"아님" + s : s;
}

// ---- Preset text --------------------------------------------------------------
// Labels are stored by display text (interned ids differ per snapshot); set
// members are stored as native ids. Tokens: '\t' separated, one node per line.
inline std::wstring escape(const std::wstring &s) {
    std::wstring o;
    for (wchar_t c : s) {
        if (c == L'\\') o += L"\\\\"; else if (c == L'\t') o += L"\\t"; else if (c == L'\n') o += L"\\n";
        else if (c == L'\r') o += L"\\r"; else o += c;
    }
    return o;
}
inline std::wstring unescape(const std::wstring &s) {
    std::wstring o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != L'\\' || i + 1 == s.size()) { o += s[i]; continue; }
        wchar_t n = s[++i];
        o += n == L't' ? L'\t' : n == L'n' ? L'\n' : n == L'r' ? L'\r' : n;
    }
    return o;
}
inline std::vector<std::wstring> split(const std::wstring &s, wchar_t sep) {
    std::vector<std::wstring> out;
    size_t at = 0;
    for (;;) {
        size_t n = s.find(sep, at);
        out.push_back(s.substr(at, n == std::wstring::npos ? std::wstring::npos : n - at));
        if (n == std::wstring::npos) break;
        at = n + 1;
    }
    return out;
}
inline void save(const Catalog &cat, const Labels &labels, const Group &g, std::wstring &out) {
    out += L"G\t" + std::wstring(g.any ? L"1" : L"0") + L"\t" + (g.negate ? L"1" : L"0") + L"\n";
    for (const auto &c : g.conditions) {
        const Field *f = cat.find(c.field);
        if (!f) continue;
        out += L"C\t" + std::to_wstring(c.field) + L"\t" + std::to_wstring(static_cast<int>(c.op)) + L"\t" +
               (c.negate ? L"1" : L"0") + L"\t" + std::to_wstring(c.a) + L"\t" + std::to_wstring(c.b) + L"\t" + escape(c.text);
        for (uint32_t v : c.values)
            out += L"\t" + (f->kind == Kind::Label ? L"s" + escape(labels.text(c.field, v)) : L"n" + std::to_wstring(v));
        out += L"\n";
    }
    for (const auto &child : g.groups) save(cat, labels, child, out);
    out += L"E\n";
}
inline std::wstring save(const Catalog &cat, const Labels &labels, const Group &g) {
    std::wstring out;
    save(cat, labels, g, out);
    return out;
}
// Returns false on malformed text. Unknown fields and label texts that are not
// present in this snapshot are dropped and counted in `dropped`.
inline bool load(const Catalog &cat, Labels &labels, const std::wstring &text, Group &out, unsigned &dropped) {
    dropped = 0;
    std::vector<Group *> stack;
    Group root;
    bool started = false, finished = false;
    for (const auto &line : split(text, L'\n')) {
        if (line.empty()) continue;
        if (finished) return false;
        auto t = split(line, L'\t');
        try {
            if (t[0] == L"G" && t.size() >= 3) {
                Group g; g.any = t[1] == L"1"; g.negate = t[2] == L"1";
                if (!started) { root = g; stack.push_back(&root); started = true; }
                else { if (stack.empty()) return false; stack.back()->groups.push_back(g); stack.push_back(&stack.back()->groups.back()); }
            } else if (t[0] == L"E") {
                if (stack.empty()) return false;
                stack.pop_back();
                if (stack.empty()) finished = true;
            } else if (t[0] == L"C" && t.size() >= 7 && !stack.empty()) {
                Condition c;
                c.field = static_cast<uint16_t>(std::stoul(t[1]));
                int op = std::stoi(t[2]);
                if (op < 0 || op > static_cast<int>(Op::Known)) return false;
                c.op = static_cast<Op>(op); c.negate = t[3] == L"1";
                c.a = std::stoll(t[4]); c.b = std::stoll(t[5]); c.text = unescape(t[6]);
                const Field *f = cat.find(c.field);
                if (!f) { ++dropped; continue; }
                bool lost = false;
                for (size_t i = 7; i < t.size(); ++i) {
                    if (t[i].empty()) continue;
                    if (t[i][0] == L's') {
                        uint32_t id = labels.find(c.field, unescape(t[i].substr(1)));
                        if (id) c.values.push_back(id); else lost = true;
                    } else if (t[i][0] == L'n') c.values.push_back(static_cast<uint32_t>(std::stoul(t[i].substr(1))));
                }
                bool needs = c.op == Op::AnyOf || c.op == Op::HasAny || c.op == Op::HasAll;
                if (lost) ++dropped;
                if (needs && c.values.empty()) continue;
                stack.back()->conditions.push_back(std::move(c));
            } else return false;
        } catch (...) { return false; }
    }
    if (!finished) return false;
    out = std::move(root);
    return true;
}

// Navigate a group by path of child-group indices (UI editing helper).
inline Group *at(Group &root, const std::vector<size_t> &path) {
    Group *g = &root;
    for (size_t i : path) { if (i >= g->groups.size()) return nullptr; g = &g->groups[i]; }
    return g;
}
} // namespace officer_query
