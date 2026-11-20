// Execute the production native guards against fake state/page/view memory.
// No game, system input, save access, hooks or DllMain execution in this test EXE.
#include "../src/ControlDynamicHUD.cpp"

static u8 testState[0x30],testOtherState[0x30],testPage[0xE0],testPlatform[0x500];
static uptr testView[1],testViewVtable[62],testPlatformPtr;
static u64 testNow;
static int nativeCalls,requests,exitCalls;
static u64 WINAPI fake_time(){return testNow;}
static void* WINAPI fake_view(void*){return testView;}
static void WINAPI fake_tick(void* state){
    ++nativeCalls;
    if(rd8((u8*)state+0x24)==1){++requests;*(u8*)((u8*)state+0x24)=0;}
}
static void WINAPI fake_exit(void*){if(g_splashSkipDone)++exitCalls;}
static void reset_test(){
    for(usize i=0;i<sizeof(testState);++i)((volatile u8*)testState)[i]=0;
    for(usize i=0;i<sizeof(testOtherState);++i)((volatile u8*)testOtherState)[i]=0;
    for(usize i=0;i<sizeof(testPage);++i)((volatile u8*)testPage)[i]=0;
    for(usize i=0;i<sizeof(testPlatform);++i)((volatile u8*)testPlatform)[i]=0;
    g_splashFlowVtable=0x1000;g_splashPageVtable=0x2000;
    *(uptr*)testState=g_splashFlowVtable;*(uptr*)testOtherState=g_splashFlowVtable;
    *(uptr*)testPage=g_splashPageVtable;*(uptr*)(testState+0x28)=(uptr)testPage;
    *(uptr*)(testOtherState+0x28)=(uptr)testPage;testPage[0xD0]=1;
    testPlatformPtr=(uptr)testPlatform;g_splashPlatformSlot=(uptr)&testPlatformPtr;
    testView[0]=(uptr)testViewVtable;
    g_coherentBase=(void*)((uptr)testViewVtable-kPublicViewVtableRva);
    testViewVtable[61]=(uptr)g_coherentBase+kExecuteScriptRva;
    g_pageGetView=&fake_view;g_GetTickCount64=&fake_time;
    g_originalSplashTick=&fake_tick;g_originalSplashExit=&fake_exit;
    g_splashFirstFlow=nullptr;g_splashSkipDone=false;g_splashWaitStart=0;g_splashReadyStart=0;
    g_cfg.startupSkipPressAnyKey=true;g_cfg.startupSkipPressAnyKeyDelayMs=1000;
    testNow=100;nativeCalls=requests=exitCalls=0;
}
extern "C" int WINAPI TestMain(){
    reset_test();g_cfg.startupSkipPressAnyKey=false;HookSplashTick(testState);testNow=2100;HookSplashTick(testState);if(requests)return 15;
    reset_test();HookSplashTick(testState);testNow=1099;HookSplashTick(testState);
    if(requests||nativeCalls!=2)return 1;
    testNow=1100;HookSplashTick(testState);if(requests!=1||!g_splashSkipDone)return 2;
    testNow=2200;HookSplashTick(testState);if(requests!=1)return 3;
    reset_test();g_cfg.startupSkipPressAnyKeyDelayMs=0;HookSplashTick(testState);if(requests!=1)return 4;
    reset_test();testPage[0xD0]=0;HookSplashTick(testState);testNow=30100;HookSplashTick(testState);
    if(requests||!g_splashSkipDone)return 5;
    reset_test();*(u32*)(testState+0x18)=1;HookSplashTick(testState);if(requests||!g_splashSkipDone)return 6;
    reset_test();HookSplashTick(testState);HookSplashExit(testState);testNow=2100;HookSplashTick(testOtherState);
    if(requests||exitCalls!=1)return 7;
    reset_test();HookSplashTick(testState);testNow=2100;HookSplashTick(testOtherState);if(requests||!g_splashSkipDone)return 8;
    reset_test();*(uptr*)testPage=0x3000;HookSplashTick(testState);testNow=2100;HookSplashTick(testState);if(requests)return 9;
    reset_test();*(u32*)(testPlatform+0x4F8)=1;HookSplashTick(testState);testNow=2100;HookSplashTick(testState);if(requests)return 10;
    reset_test();testViewVtable[61]=0;HookSplashTick(testState);testNow=2100;HookSplashTick(testState);if(requests)return 11;
    reset_test();HookSplashTick(testState);testPage[0xD0]=0;testNow=200;HookSplashTick(testState);testPage[0xD0]=1;testNow=2100;HookSplashTick(testState);if(requests||!g_splashSkipDone)return 12;
    reset_test();HookSplashTick(testState);testNow=50;HookSplashTick(testState);if(requests||!g_splashSkipDone)return 13;
    // If the game already requested progression, the mod does not request it again.
    reset_test();testState[0x24]=1;HookSplashTick(testState);if(requests!=1||!g_splashSkipDone)return 14;
    return 0;
}
