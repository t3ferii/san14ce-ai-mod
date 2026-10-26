// Read-only officer snapshot for the standalone searcher (OfficerSearch.exe).
// Runs only from a 통합 인사 menu click on the UI thread. Every cell comes from
// the native officer list column getters (the same text the 무장일람 shows);
// each getter call is guarded so one odd officer cannot take the game down.
// Output: AI_TacticsMod\data\officer_snapshot.tsv and officer_snapshot_tabs.tsv
namespace unified_native {
static bool snapshot_value_call(Obj get,Obj person,NativeColumnValue *out) {
    __try {reinterpret_cast<NativeColumnValue *(__fastcall *)(NativeColumnValue *,Obj)>(get)(out,person);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static bool snapshot_text_call(Obj get,Obj person,const wchar_t **out) {
    __try {*out=reinterpret_cast<const wchar_t *(__fastcall *)(Obj)>(get)(person);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static bool snapshot_caption_call(int message,const wchar_t **out) {
    __try {*out=fn<const wchar_t *(__fastcall *)(int)>(0x30CF80)(message);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static std::wstring snapshot_clean(const wchar_t *text) {
    std::wstring s=text?std::wstring(text,wcsnlen(text,512)):std::wstring();
    for(auto &c:s)if(c==L'\t' || c==L'\r' || c==L'\n')c=L' ';
    return search_trim(s);
}
// One cell: numeric value when the column's value getter gives a number,
// otherwise the column text. Unknown/hidden cells stay empty.
static std::wstring snapshot_cell(Obj person,unsigned column,bool &numeric) {
    numeric=false;
    if(Obj get=search_column_function(static_cast<int>(column),0x28)) {
        NativeColumnValue v{};v.kind=2;
        if(snapshot_value_call(get,person,&v)) {
            if(v.kind==0){numeric=true;return std::to_wstring(v.value.number);}
            if(v.kind==1)return snapshot_clean(v.value.text);
        }
    }
    if(Obj get=search_column_function(static_cast<int>(column),0x18)) {
        const wchar_t *text=nullptr;if(snapshot_text_call(get,person,&text))return snapshot_clean(text);
    }
    return {};
}
static std::wstring snapshot_caption(unsigned column) {
    Obj col=image+0x1A2B850+column*0x60;int message=static_cast<int>(dword(col,0x48));
    const wchar_t *text=nullptr;
    if(message>=0 && message<=0xBB7 && snapshot_caption_call(message,&text) && text && *text)return snapshot_clean(text);
    return L"열"+std::to_wstring(column);
}
static bool snapshot_write(const std::wstring &path,const std::wstring &text) {
    std::string bytes="\xEF\xBB\xBF"+search_utf8(text);std::wstring temp=path+L".tmp";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool done=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size();
    CloseHandle(file);return done && MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
static std::wstring snapshot_join(const std::vector<std::wstring> &values) {
    std::wstring s;for(const auto &v:values){if(v.empty())continue;if(!s.empty())s+=L"|";s+=v;}return s;
}
// Returns the number of officers written, or -1 on a write failure.
static int snapshot_export() {
    auto base=search_preset_path();auto slash=base.find_last_of(L"\\/");if(base.empty() || slash==std::wstring::npos)return -1;
    std::wstring folder=base.substr(0,slash);
    // Columns: every native 무장일람 tab, then our extra tabs, then the search fields.
    std::vector<unsigned> order;std::set<unsigned> seen;
    auto add=[&](int c){if(c>=0 && c<213 && seen.insert(static_cast<unsigned>(c)).second)order.push_back(static_cast<unsigned>(c));};
    std::wstring tabs=L"#탭\t열 번호 (officer_snapshot.tsv 의 c번호)\n";
    for(unsigned i=0;i<16;++i) {
        auto tab=*reinterpret_cast<const unified_dialogs::Tab *>(image+kInfoTabs+i*sizeof(unified_dialogs::Tab));
        if(!tab.columns)break;
        const wchar_t *caption=nullptr;std::wstring name;
        if(tab.message>=0 && snapshot_caption_call(tab.message,&caption) && caption && *caption)name=snapshot_clean(caption);
        if(name.empty())name=L"탭"+std::to_wstring(i+1);
        std::wstring list;
        for(unsigned k=0;k<40 && tab.columns[k]>=0;++k){add(tab.columns[k]);if(!list.empty())list+=L",";list+=std::to_wstring(tab.columns[k]);}
        tabs+=name+L"\t"+list+L"\n";
    }
    for(const auto &t:search_tabs)for(int c:t){if(c<0)break;add(c);}
    for(const auto &f:search_fields)add(f.column);
    // Header lines: keys, captions, kinds (N number / T text, decided after the rows).
    std::vector<bool> numeric(order.size(),false);
    std::wstring rows;int count=0;
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);if(!valid(p))continue;
        std::wstring line=std::to_wstring(word(p,0x10))+L"\t"+snapshot_clean(officer_name(p).c_str())+L"\t"+std::to_wstring(static_cast<unsigned>(p[0xE6]));
        for(size_t i=0;i<order.size();++i) {
            bool number=false;std::wstring cell=snapshot_cell(p,order[i],number);
            if(number)numeric[i]=true;line+=L"\t"+cell;
        }
        std::vector<std::wstring> traits,tactics,formations,friends;
        for(unsigned i=0;i<9;++i) {
            unsigned t=word(p,0x118+i*2);
            if(t && t<sizeof(unified_labels::personalities)/sizeof(*unified_labels::personalities))traits.push_back(unified_labels::personalities[t]);
        }
        for(unsigned i=0;i<10;++i){unsigned t=p[0x130+i];if(t && t<sizeof(unified_labels::tactics)/sizeof(*unified_labels::tactics))tactics.push_back(unified_labels::tactics[t]);}
        for(unsigned i=1;i<=14;++i)
            if(fn<int(__fastcall *)(Obj,unsigned)>(0x232350)(p,i) && i<sizeof(unified_labels::formations)/sizeof(*unified_labels::formations))
                formations.push_back(unified_labels::formations[i]);
        for(unsigned i=0;i<8;++i) {
            unsigned f=word(p,0xC0+i*2);Obj o=f && f<=6000?table(0x158,f,6000):nullptr;
            if(valid(o))friends.push_back(snapshot_clean(officer_name(o).c_str()));
        }
        line+=L"\t"+snapshot_join(traits)+L"\t"+snapshot_join(tactics)+L"\t"+snapshot_join(formations)+L"\t"+snapshot_join(friends);
        rows+=line+L"\n";++count;
    }
    std::wstring keys=L"id\tname\tstatus",captions=L"번호\t이름\t상태코드",kinds=L"N\tT\tN";
    for(size_t i=0;i<order.size();++i) {
        keys+=L"\tc"+std::to_wstring(order[i]);captions+=L"\t"+snapshot_caption(order[i]);kinds+=numeric[i]?L"\tN":L"\tT";
    }
    keys+=L"\ttraits\ttactics\tformations\tfriends";captions+=L"\t개성\t전법\t진형\t친애 무장";kinds+=L"\tS\tS\tS\tS";
    SYSTEMTIME now{};GetLocalTime(&now);
    wchar_t stamp[64];swprintf_s(stamp,L"%04u-%02u-%02u %02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute);
    std::wstring head=L"#SAN14 officer snapshot\t1\t"+std::wstring(stamp)+L"\t"+std::to_wstring(count)+L"\n";
    std::wstring text=head+keys+L"\n"+captions+L"\n"+kinds+L"\n"+rows;
    if(!snapshot_write(folder+L"\\officer_snapshot_tabs.tsv",tabs))return -1;
    return snapshot_write(folder+L"\\officer_snapshot.tsv",text)?count:-1;
}
}
