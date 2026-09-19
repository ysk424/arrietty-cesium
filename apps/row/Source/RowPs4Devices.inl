// Included inside row namespace after Shared. Windows HID only, explicit device.
static void ps4Loop(std::shared_ptr<Shared> s,std::string path) {
    if(path.empty()) return;
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0);
    if(n<=0) return;
    std::vector<wchar_t> wide(size_t(n),0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,wide.data(),n);
    Ps4Fusion fusion;Ps4Buttons buttons;
    while(!s->stop) {
        HANDLE device=CreateFileW(wide.data(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        HANDLE event=nullptr;
        if(device!=INVALID_HANDLE_VALUE) {
            HIDD_ATTRIBUTES attributes{};attributes.Size=sizeof(attributes);
            PHIDP_PREPARSED_DATA data=nullptr;HIDP_CAPS caps{};
            bool usable=HidD_GetAttributes(device,&attributes) && attributes.VendorID==0x054c &&
                (attributes.ProductID==0x09cc || attributes.ProductID==0x05c4) && HidD_GetPreparsedData(device,&data);
            if(usable) { usable=HidP_GetCaps(data,&caps)==HIDP_STATUS_SUCCESS;HidD_FreePreparsedData(data); }
            usable=usable && caps.UsagePage==1 && caps.Usage==5 && caps.InputReportByteLength>=78 && caps.InputReportByteLength<=4096;
            auto enable=ps4EnableReport(caps.OutputReportByteLength);
            if(usable && !enable.empty()) usable=HidD_SetOutputReport(device,enable.data(),ULONG(enable.size()))!=0;
            else usable=false;
            if(usable) {
                HidD_FlushQueue(device);event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
                std::vector<uint8_t> buffer(caps.InputReportByteLength);
                double lastValid=Devices::seconds();uint16_t lastTick=0;bool haveTick=false;
                while(event && !s->stop) {
                    OVERLAPPED op{};op.hEvent=event;ResetEvent(event);DWORD count=0;
                    BOOL done=ReadFile(device,buffer.data(),DWORD(buffer.size()),&count,&op);
                    if(!done && GetLastError()!=ERROR_IO_PENDING) break;
                    while(!done && !s->stop) {
                        const DWORD status=WaitForSingleObject(event,25);
                        if(status==WAIT_OBJECT_0) {done=GetOverlappedResult(device,&op,&count,FALSE);break;}
                        if(status!=WAIT_TIMEOUT || Devices::seconds()-lastValid>1) break;
                    }
                    if(!done) {CancelIoEx(device,&op);GetOverlappedResult(device,&op,&count,TRUE);break;}
                    const double now=Devices::seconds();Ps4Packet packet;
                    if(!parsePs4(buffer.data(),count,packet)) {
                        std::lock_guard lock(s->mutex);++s->data.imuRejected;
                        if(now-lastValid>1) break;else continue;
                    }
                    if(haveTick && packet.tick==lastTick) { if(now-lastValid>1) break;continue; }
                    haveTick=true;lastTick=packet.tick;
                    if(now-lastValid>=.25) {
                        // Discard accumulated reports after a worker scheduling
                        // gap; queued packets are not evidence of current motion.
                        buttons.disconnect();HidD_FlushQueue(device);haveTick=false;
                        {std::lock_guard lock(s->mutex);s->data.imu.valid=false;s->data.ps4=buttons.state;}
                        lastValid=now;continue;
                    }
                    if(!fusion.tick(packet,now)) continue;
                    lastValid=now;buttons.tick(packet,now);
                    std::lock_guard lock(s->mutex);s->data.imu=fusion.sample;s->data.ps4=buttons.state;s->data.imuConnected=true;
                }
            }
            if(event) CloseHandle(event);CloseHandle(device);
        }
        buttons.disconnect();
        {std::lock_guard lock(s->mutex);s->data.imu.valid=false;s->data.imuConnected=false;s->data.ps4=buttons.state;++s->data.imuErrors;}
        // Keep the learned frame and gyro orientation across reconnects. No
        // elapsed gap is integrated. The host resumes only previously active rides.
        for(int i=0;i<5 && !s->stop;++i) std::this_thread::sleep_for(20ms);
    }
}
