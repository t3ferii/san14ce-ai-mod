static void crash_diagnostic_tests() {
    LONG previous_logging=diagnostic_logging;diagnostic_logging=1; // This fixture explicitly exercises development diagnostics.
    wchar_t saved[MAX_PATH],directory[MAX_PATH],temporary[MAX_PATH];
    wcscpy_s(saved,log_path);
    check(GetTempPathW(MAX_PATH,directory)>0 && GetTempFileNameW(directory,L"aic",0,temporary)!=0,
          "diagnostic fixture uses its own temporary log");
    wcscpy_s(log_path,temporary);crash_records=0;crash_image_size=0x02A0B000;
    EXCEPTION_RECORD record={};record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    record.ExceptionAddress=image+0x1A00EE;record.NumberParameters=2;
    record.ExceptionInformation[0]=1;record.ExceptionInformation[1]=0x1234;
    CONTEXT context={};context.Rax=0x11;context.Rdi=0x22;context.R15=0x33;context.Rip=reinterpret_cast<uintptr_t>(record.ExceptionAddress);
    EXCEPTION_POINTERS pointers={&record,&context};
    {
        NativePhase outer("domestic_tick");
        {NativePhase inner("monthly_population");
         check(observe_native_exception(&pointers)==EXCEPTION_CONTINUE_SEARCH,
               "diagnostic never handles or suppresses an access violation");}
        check(strcmp(native_phase,"domestic_tick")==0,"nested callback diagnostic restores enclosing phase");
    }
    check(strcmp(native_phase,"outside_mod_callback")==0,"completed callback restores idle diagnostic phase");
    HANDLE file=CreateFileW(temporary,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    LARGE_INTEGER length={};DWORD read=0;std::vector<char> contents;
    bool readable=file!=INVALID_HANDLE_VALUE && GetFileSizeEx(file,&length) && length.QuadPart>=0;
    if(readable)contents.resize(static_cast<size_t>(length.QuadPart)+1,0);
    readable=readable && ReadFile(file,contents.data(),static_cast<DWORD>(contents.size()-1),&read,nullptr);
    check(readable,"diagnostic record readable");
    CloseHandle(file);
    check(strstr(contents.data(),"code=0x00000000C0000005") && strstr(contents.data(),"game_rva=0x00000000001A00EE") &&
          strstr(contents.data(),"phase=monthly_population") && strstr(contents.data(),"address=0x0000000000001234") &&
          strstr(contents.data(),"rax=0x0000000000000011") && strstr(contents.data(),"rdi=0x0000000000000022") &&
          strstr(contents.data(),"r15=0x0000000000000033") && strstr(contents.data(),"native_exception_widget") &&
          strstr(contents.data(),"unit_id=0x0000000000000000") && strstr(contents.data(),"display_readable=0x0000000000000000"),
          "synthetic fault records registers and safely zeroes unreadable widget fields");
    record.ExceptionCode=0xE06D7363;
    check(observe_native_exception(&pointers)==EXCEPTION_CONTINUE_SEARCH && crash_records==1,
          "ordinary C++ exception ignored without consuming diagnostic budget");
    record.ExceptionCode=EXCEPTION_STACK_OVERFLOW;
    check(observe_native_exception(&pointers)==EXCEPTION_CONTINUE_SEARCH && crash_records==1,
          "stack overflow avoids unreserved stack logging");
    record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    for(unsigned i=0;i<4;++i)observe_native_exception(&pointers);
    file=CreateFileW(temporary,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    length.QuadPart=0;read=0;readable=file!=INVALID_HANDLE_VALUE && GetFileSizeEx(file,&length) && length.QuadPart>=0;
    if(readable)contents.assign(static_cast<size_t>(length.QuadPart)+1,0);
    readable=readable && ReadFile(file,contents.data(),static_cast<DWORD>(contents.size()-1),&read,nullptr);CloseHandle(file);
    unsigned records=0;const char *cursor=contents.data();
    while((cursor=strstr(cursor,"native_exception_first_chance code="))!=nullptr){++records;++cursor;}
    check(readable && records==3,"first chance diagnostic is bounded to three exception records");
    check(DeleteFileW(temporary)!=0,"fixture removes only its own temporary log");
    wcscpy_s(log_path,saved);crash_records=0;crash_image_size=0;
    diagnostic_logging=previous_logging;
}
