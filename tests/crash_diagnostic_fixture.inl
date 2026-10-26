static void crash_diagnostic_tests() {
    LONG previous_logging=diagnostic_logging;diagnostic_logging=1; // This fixture explicitly exercises development diagnostics.
    wchar_t saved[MAX_PATH],directory[MAX_PATH],temporary[MAX_PATH];
    wcscpy_s(saved,log_path);
    check(GetTempPathW(MAX_PATH,directory)>0 && GetTempFileNameW(directory,L"aic",0,temporary)!=0,
          "diagnostic fixture uses its own temporary log");
    wcscpy_s(log_path,temporary);crash_records=0;crash_image_size=0x029FF000;
    EXCEPTION_RECORD record={};record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    record.ExceptionAddress=image+0x302980;record.NumberParameters=2;
    record.ExceptionInformation[0]=1;record.ExceptionInformation[1]=0x1234;
    EXCEPTION_POINTERS pointers={&record,nullptr};
    {
        NativePhase outer("domestic_tick");
        {NativePhase inner("monthly_population");
         check(observe_native_exception(&pointers)==EXCEPTION_CONTINUE_SEARCH,
               "diagnostic never handles or suppresses an access violation");}
        check(strcmp(native_phase,"domestic_tick")==0,"nested callback diagnostic restores enclosing phase");
    }
    check(strcmp(native_phase,"outside_mod_callback")==0,"completed callback restores idle diagnostic phase");
    HANDLE file=CreateFileW(temporary,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    char contents[2048]={};DWORD read=0;
    check(file!=INVALID_HANDLE_VALUE && ReadFile(file,contents,sizeof(contents)-1,&read,nullptr),"diagnostic record readable");
    CloseHandle(file);
    check(strstr(contents,"code=0x00000000C0000005") && strstr(contents,"game_rva=0x0000000000302980") &&
          strstr(contents,"phase=monthly_population") && strstr(contents,"address=0x0000000000001234"),
          "synthetic fault records exact code game RVA phase and accessed address");
    record.ExceptionCode=0xE06D7363;
    check(observe_native_exception(&pointers)==EXCEPTION_CONTINUE_SEARCH && crash_records==1,
          "ordinary C++ exception ignored without consuming diagnostic budget");
    record.ExceptionCode=EXCEPTION_STACK_OVERFLOW;
    check(observe_native_exception(&pointers)==EXCEPTION_CONTINUE_SEARCH && crash_records==1,
          "stack overflow avoids unreserved stack logging");
    record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    for(unsigned i=0;i<4;++i)observe_native_exception(&pointers);
    file=CreateFileW(temporary,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    memset(contents,0,sizeof(contents));ReadFile(file,contents,sizeof(contents)-1,&read,nullptr);CloseHandle(file);
    unsigned lines=0;for(unsigned i=0;i<read;++i)if(contents[i]=='\n')++lines;
    check(lines==3,"first chance diagnostic is bounded to three records");
    check(DeleteFileW(temporary)!=0,"fixture removes only its own temporary log");
    wcscpy_s(log_path,saved);crash_records=0;crash_image_size=0;
    diagnostic_logging=previous_logging;
}
