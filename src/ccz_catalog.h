#pragma once
#include <algorithm>
#include <array>
#include <vector>
#include <string>
namespace ccz_catalog {
constexpr unsigned original_base=4096,source_cells=1500,raw_size=48*48*11+64*64*12+48*48*5;
struct Entry {unsigned short cell,source;unsigned offset,packed,raw,thumbnail,thumbnail_size;};
static_assert(sizeof(Entry)==24,"catalog disk entry");
class Reader {
    HANDLE file=INVALID_HANDLE_VALUE;mutable SRWLOCK io=SRWLOCK_INIT;
    unsigned long long length=0;
    bool read(unsigned offset,void *out,unsigned bytes)const{
        if(file==INVALID_HANDLE_VALUE || static_cast<unsigned long long>(offset)+bytes>length)return false;
        AcquireSRWLockExclusive(&io);LARGE_INTEGER at{};at.QuadPart=offset;DWORD got=0;
        bool ok=SetFilePointerEx(file,at,nullptr,FILE_BEGIN) && ReadFile(file,out,bytes,&got,nullptr) && got==bytes;
        ReleaseSRWLockExclusive(&io);return ok;
    }
public:
    std::array<std::array<unsigned char,1024>,2> palettes{};std::vector<Entry> entries;
    Reader()=default;Reader(const Reader &)=delete;Reader &operator=(const Reader &)=delete;
    ~Reader(){close();}
    void close(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);file=INVALID_HANDLE_VALUE;entries.clear();length=0;}
    bool open(const wchar_t *path){
        close();file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
        LARGE_INTEGER size{};if(!GetFileSizeEx(file,&size) || size.QuadPart<2064 || size.QuadPart>0x40000000){close();return false;}length=size.QuadPart;
        unsigned header[4]={};if(!read(0,header,16) || header[0]!=0x435A4343u || header[1]!=1 || header[2]>3000 || header[3]!=2064 || 2064ull+24ull*header[2]>length){close();return false;}
        entries.resize(header[2]);if(!read(16,palettes.data(),2048) || !read(2064,entries.data(),static_cast<unsigned>(entries.size()*24))){close();return false;}
        unsigned previous=0;bool first=true;
        for(const auto &e:entries){
            if((!first && e.cell<=previous) || e.source>1 || e.cell/4096!=e.source || e.cell%4096>=source_cells || e.raw!=raw_size || !e.packed || e.packed>raw_size*2 || e.offset<2064+entries.size()*24 || static_cast<unsigned long long>(e.offset)+e.packed>length || e.thumbnail_size!=2304 || static_cast<unsigned long long>(e.thumbnail)+2304>length){close();return false;}
            previous=e.cell;first=false;
        }
        return !entries.empty();
    }
    const Entry *find(unsigned cell)const{auto p=std::lower_bound(entries.begin(),entries.end(),cell,[](const Entry &e,unsigned id){return e.cell<id;});return p!=entries.end() && p->cell==cell?&*p:nullptr;}
    bool preview(unsigned cell,std::array<unsigned char,2304> &out)const{auto e=find(cell);return e && read(e->thumbnail,out.data(),2304);}
    bool decode(unsigned cell,std::vector<unsigned char> &out)const{
        auto e=find(cell);if(!e)return false;std::vector<unsigned char> packed(e->packed);if(!read(e->offset,packed.data(),e->packed))return false;
        out.clear();out.reserve(raw_size);size_t at=0;
        while(at<packed.size()){
            unsigned b=packed[at++],n=b<128?b+1:b-127;if(out.size()+n>raw_size)return false;
            if(b<128){if(at+n>packed.size())return false;out.insert(out.end(),packed.data()+at,packed.data()+at+n);at+=n;}
            else out.insert(out.end(),n,0);
        }
        return out.size()==raw_size;
    }
    size_t resident_bytes()const{return palettes.size()*1024+entries.size()*sizeof(Entry);}
};
}
