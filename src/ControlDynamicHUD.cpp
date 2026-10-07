// Control Dynamic HUD - V0.2 HUD lifecycle probe
// Freestanding x64 DLL for Control Plugin Loader. No visual HUD changes.

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using s32 = int;
using s64 = long long;
using u64 = unsigned long long;
using uptr = unsigned long long;
using usize = unsigned long long;
using BOOL = int;
using DWORD = unsigned long;
using HANDLE = void*;
using HMODULE = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPCWSTR = const wchar_t*;

#define WINAPI __attribute__((ms_abi))
#define DLL_PROCESS_ATTACH 1

extern "C" int _fltused = 0;

static constexpr DWORD GENERIC_WRITE = 0x40000000u;
static constexpr DWORD FILE_SHARE_READ = 0x1u;
static constexpr DWORD FILE_SHARE_WRITE = 0x2u;
static constexpr DWORD CREATE_ALWAYS = 2u;
static constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x80u;
static constexpr DWORD PAGE_EXECUTE_READWRITE = 0x40u;
static constexpr DWORD MEM_COMMIT = 0x1000u;
static constexpr DWORD MEM_RESERVE = 0x2000u;
static constexpr uptr INVALID_HANDLE_VALUE_U = ~uptr(0);

struct UNICODE_STRING_T { u16 Length; u16 MaximumLength; wchar_t* Buffer; };
struct LIST_ENTRY_T { LIST_ENTRY_T* Flink; LIST_ENTRY_T* Blink; };

using FnCreateFileW = HANDLE (WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
using FnWriteFile = BOOL (WINAPI*)(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
using FnCloseHandle = BOOL (WINAPI*)(HANDLE);
using FnVirtualAlloc = LPVOID (WINAPI*)(LPVOID,usize,DWORD,DWORD);
using FnVirtualProtect = BOOL (WINAPI*)(LPVOID,usize,DWORD,DWORD*);
using FnFlushInstructionCache = BOOL (WINAPI*)(HANDLE,LPCVOID,usize);
using FnGetPrivateProfileIntA = u32 (WINAPI*)(const char*,const char*,s32,const char*);
using FnGetPrivateProfileStringA = DWORD (WINAPI*)(const char*,const char*,const char*,char*,DWORD,const char*);
using FnGetAsyncKeyState = short (WINAPI*)(int);
using FnGetTickCount64 = u64 (WINAPI*)();

typedef void* (WINAPI *FactoryFn)(void*, void*, void*, void*);
typedef void* (WINAPI *PageGetViewFn)(void*);
typedef void (WINAPI *ExecuteScriptFn)(void*, const char*, const char*);
typedef void (WINAPI *ReadyFn)(void*);
typedef void (WINAPI *UpdateFn)(void*);
typedef void (WINAPI *MultiLaunchUpdateFn)(void*,u32,void*,void*);
typedef void (WINAPI *CrosshairUpdateFn)(void*,void*,void*);
typedef void* (WINAPI *MenuFactoryFn)(void*);

static HANDLE g_log = nullptr;
static FnWriteFile g_WriteFile = nullptr;
static FactoryFn g_originalFactory = nullptr;
static PageGetViewFn g_pageGetView = nullptr;
static ReadyFn g_originalReady = nullptr;
static UpdateFn g_originalUpdate = nullptr;
static void* g_coherentBase = nullptr;
static uptr* g_readyEntry = nullptr;
static uptr* g_updateEntry = nullptr;
static FnVirtualProtect g_VirtualProtect = nullptr;
static FnFlushInstructionCache g_FlushInstructionCache = nullptr;
static FnGetAsyncKeyState g_GetAsyncKeyState = nullptr;
static FnGetTickCount64 g_GetTickCount64 = nullptr;
static u32 g_showHudVk = 0;
static u64 g_forceHudUntilMs = 0;
static u64 g_launchReticleSinceMs = 0;
static u64 g_multiLaunchActiveSinceMs = 0;
static bool g_multiLaunchSlotActive[3] = {false,false,false};
static bool g_multiLaunchPatchEnabled = false;
static bool g_launchReticleLogOnce = false;
static bool g_multiLaunchReticleLogOnce = false;
static uptr* g_interfaceOptionsPtrSlot = nullptr;
static bool g_interfaceOptionsSaved = false;
static u8 g_savedTargetIndicator = 1;
static bool g_targetIndicatorOverrideActive = false;
static u8* g_launchChangeDispatch[2] = {nullptr,nullptr};
static u8 g_launchChangeOriginal[2][6] = {};
static u8* g_launchStartTail = nullptr;
static u8 g_launchStartOriginal[7] = {};
static bool g_launchSelectionSuppressed = false;

static MultiLaunchUpdateFn g_originalMultiLaunchUpdate = nullptr;
static CrosshairUpdateFn g_originalCrosshairUpdate = nullptr;
static u8* g_multiHiddenCmpImm = nullptr;
static u8* g_multiHiddenMovImm = nullptr;
static u8* g_multiHasAimForceBytes = nullptr;
static bool g_multiLaunchDiagLogged[3] = {false,false,false};
static u64 g_crosshairDotSinceMsNative = 0;
static bool g_crosshairDotNativeLogOnce = false;
static bool g_missionSelectModelPatched = false;
static bool g_showHudKeyDown = false;
static volatile u32 g_factoryCalls = 0;
static volatile u32 g_readyCalls = 0;
static MenuFactoryFn g_originalMenuFactory = nullptr;
static ReadyFn g_originalMenuReady = nullptr;
static uptr* g_menuReadyEntry = nullptr;
static volatile u32 g_menuReadyCalls = 0;

struct Config {
    bool enabled;
    bool diagnostics;
    char showHudKey[16];
    u32 showHudDurationMs;

    bool healthEnabled;
    u32 healthHideDelayMs;
    u32 healthFadeDurationMs;
    u32 healthOpacityPercent;
    bool healthShowDuringCombat;
    u32 healthThresholdPercent;

    bool missionEnabled;
    u32 missionInitialHideDelayMs;
    u32 missionAfterMapCloseHideDelayMs;
    u32 missionUpdateVisibleMs;
    u32 missionFadeDurationMs;
    bool missionShowInMap;

    bool crosshairEnabled;
    u32 crosshairHideDelayMs;
    u32 crosshairFadeDurationMs;

    bool crosshairDotEnabled;
    u32 crosshairDotHideDelayMs;

    bool expeditionEnabled;
    u32 expeditionHideDelayMs;
    u32 expeditionFadeDurationMs;

    bool hideGroundSlamTargetCircle;

    bool launchHideReticle;
    u32 launchHideDelayMs;

    bool multiLaunchHideObjectIndicators;
    u32 multiLaunchHideDelayMs;

    bool mainMenuHideNewGame;
    bool mainMenuHideMissionSelect;
};
static Config g_cfg{};
static char g_configScript[2048];
static char g_menuConfigScript[256];

static constexpr u32 kCoherentTimeDateStamp = 0x5E8F6E9A;
static constexpr u32 kCoherentSizeOfImage = 0x329000;
static constexpr u32 kCoherentCheckSum = 0x00323A4A;
static constexpr uptr kPublicViewVtableRva = 0x270780;
static constexpr uptr kExecuteScriptRva = 0x82870;
static constexpr uptr kViewPageOffset = 0xA8;

static const char kMainMenuScript[] = R"JS(
(function(){
 var CFG=window.__CDH_MAINMENU_CONFIG||{hideNewGame:0,hideMissionSelect:0};
 if(!CFG.hideNewGame&&!CFG.hideMissionSelect)return;
 if(window.__ControlDynamicHUDMainMenu&&window.__ControlDynamicHUDMainMenu.version==='1.0S')return;
 window.__ControlDynamicHUDMainMenu={version:'1.0S'};

 var style=document.getElementById('cdh-mainmenu-style');
 if(!style){style=document.createElement('style');style.id='cdh-mainmenu-style';(document.head||document.documentElement).appendChild(style);}
 style.textContent='[data-cdh-mainmenu-hidden="1"]{display:none!important;height:0!important;min-height:0!important;max-height:0!important;margin:0!important;padding:0!important;border:0!important;overflow:hidden!important;opacity:0!important;pointer-events:none!important;}';

 function norm(s){
  return String(s||'').toLowerCase()
   .replace(/[éèêë]/g,'e').replace(/[àâä]/g,'a')
   .replace(/[îï]/g,'i').replace(/[ôö]/g,'o')
   .replace(/[ùûü]/g,'u').replace(/ç/g,'c')
   .replace(/[_-]+/g,' ').replace(/\s+/g,' ').trim();
 }
 function sigFor(e){
  if(!e)return '';
  var out=' '+norm(e.textContent)+' ';
  var names=['id','class','role','aria-label','title','data-bind','data-action','data-event','data-command','onclick'];
  for(var i=0;i<names.length;i++){try{out+=' '+norm(e.getAttribute&&e.getAttribute(names[i]));}catch(x){}}
  return out;
 }
 function targetKind(e){
  var s=sigFor(e);
  if(CFG.hideNewGame){
   if(s.indexOf('onnewgameclicked')>=0||s.indexOf('newgame')>=0||
      s.indexOf(' new game ')>=0||s.indexOf(' nouvelle partie ')>=0||
      s.indexOf(' neues spiel ')>=0||s.indexOf(' nuova partita ')>=0||
      s.indexOf(' nueva partida ')>=0||s.indexOf(' novo jogo ')>=0||
      s.indexOf(' nowa gra ')>=0||s.indexOf(' новая игра ')>=0)return 'new';
  }
  if(CFG.hideMissionSelect){
   if(s.indexOf('onmissionselected')>=0||s.indexOf('missionselect')>=0||
      s.indexOf(' mission select ')>=0||s.indexOf(' select mission ')>=0||
      s.indexOf(' mission selection ')>=0||s.indexOf(' selection de mission ')>=0||
      s.indexOf(' missionsauswahl ')>=0||s.indexOf(' selezione missione ')>=0||
      s.indexOf(' seleccion de mision ')>=0||s.indexOf(' selecao de missao ')>=0)return 'mission';
  }
  return '';
 }
 function interactiveScore(e){
  if(!e)return 0;
  var tag=String(e.tagName||'').toLowerCase(),s=sigFor(e),n=0;
  if(tag==='button'||tag==='a'||tag==='li')n+=8;
  if(s.indexOf(' button ')>=0||s.indexOf(' menu ')>=0||s.indexOf(' option ')>=0||
     s.indexOf(' item ')>=0||s.indexOf(' entry ')>=0)n+=5;
  if(e.getAttribute&&e.getAttribute('role')==='button')n+=8;
  if(e.getAttribute&&e.getAttribute('tabindex')!==null)n+=3;
  if(s.indexOf('onclick')>=0||s.indexOf('data bind')>=0)n+=2;
  return n;
 }
 function rowFor(e,kind){
  var best=e,bestScore=interactiveScore(e),n=e;
  for(var i=0;i<8&&n&&n.parentElement;i++){
   var p=n.parentElement,ps=interactiveScore(p);
   if(targetKind(p)===kind&&ps>=bestScore){best=p;bestScore=ps;}
   else if(ps>bestScore&&ps>=5){best=p;bestScore=ps;}
   n=p;
  }
  return best;
 }
 function siblingMenuBranches(parent,node){
  if(!parent||!parent.children)return 0;
  var n=0;
  for(var i=0;i<parent.children.length;i++){
   var c=parent.children[i];if(c===node)continue;
   if(interactiveScore(c)>=5){n++;continue;}
   try{
    var q=c.querySelectorAll('button,a,[role="button"],[tabindex]');
    if(q&&q.length)n++;
   }catch(x){}
  }
  return n;
 }
 function collapseRowFor(e,kind){
  var row=rowFor(e,kind),best=row,n=row;
  for(var i=0;i<6&&n&&n.parentElement;i++){
   var p=n.parentElement;
   if(!p||p===document.body||p===document.documentElement)break;
   if(siblingMenuBranches(p,n)>0)break;
   if(targetKind(p)===kind)best=p;
   n=p;
  }
  return best;
 }
 function removeNewGameBranch(row){
  if(!row||!row.parentNode)return;
  var p=row.parentNode;
  try{p.removeChild(row);}catch(x){return;}
  for(var depth=0;depth<5&&p&&p!==document.body&&p!==document.documentElement;depth++){
   var next=p.parentNode;
   var txt=norm(p.textContent);
   var interactive=0;
   try{interactive=p.querySelectorAll('button,a,[role="button"],[tabindex]').length;}catch(x2){}
   if(p.children.length===0||(txt===''&&interactive===0)){
    try{if(next)next.removeChild(p);}catch(x3){}
    p=next;
   }else break;
  }
 }
 function hideTargets(){
  if(!document.body)return;
  var all=document.body.querySelectorAll('*');
  for(var i=0;i<all.length;i++){
   var e=all[i];
   if(!e||e.isConnected===false)continue;
   var kind=targetKind(e);
   if(!kind)continue;
   var row=collapseRowFor(e,kind);
   if(!row||row===document.body||row===document.documentElement||!row.parentNode)continue;
   if(kind==='new'){
    removeNewGameBranch(row);
    continue;
   }
   row.setAttribute('data-cdh-mainmenu-hidden','1');
   row.setAttribute('data-cdh-mainmenu-kind',kind);
   row.setAttribute('aria-hidden','true');
   row.setAttribute('aria-disabled','true');
   row.setAttribute('tabindex','-1');
  }
 }
 function looksSelected(row){
  if(!row)return false;
  var a=document.activeElement;
  if(a&&(a===row||(row.contains&&row.contains(a))))return true;
  var nodes=[row],q=row.querySelectorAll?row.querySelectorAll('*'):[];
  for(var i=0;i<q.length&&i<32;i++)nodes.push(q[i]);
  for(var j=0;j<nodes.length;j++){
   var e=nodes[j],sg=sigFor(e)+' '+norm(e.getAttribute&&e.getAttribute('aria-selected'))+
    ' '+norm(e.getAttribute&&e.getAttribute('data-selected'))+
    ' '+norm(e.getAttribute&&e.getAttribute('data-active'))+
    ' '+norm(e.getAttribute&&e.getAttribute('data-focused'));
   if(/selected|highlight|focused|current| active | true /.test(sg))return true;
  }
  return false;
 }
 function hiddenSelected(){
  var rows=document.querySelectorAll('[data-cdh-mainmenu-hidden="1"]');
  for(var i=0;i<rows.length;i++){
   var row=rows[i];
   if(looksSelected(row))return true;
   var rr;try{rr=row.getBoundingClientRect();}catch(x){rr=null;}
   if(!rr||rr.width<1||rr.height<1)continue;
   var all=document.body?document.body.querySelectorAll('*'):[];
   for(var j=0;j<all.length&&j<512;j++){
    var e=all[j];if(e===row||(row.contains&&row.contains(e)))continue;
    var sg=sigFor(e)+' '+norm(e.getAttribute&&e.getAttribute('aria-selected'))+
      ' '+norm(e.getAttribute&&e.getAttribute('data-selected'))+
      ' '+norm(e.getAttribute&&e.getAttribute('data-active'))+
      ' '+norm(e.getAttribute&&e.getAttribute('data-focused'));
    if(!/selected|highlight|focused|current| active | true /.test(sg))continue;
    var er;try{er=e.getBoundingClientRect();}catch(x2){continue;}
    var cy=er.top+er.height*0.5;
    if(er.width>1&&er.height>1&&cy>=rr.top-3&&cy<=rr.bottom+3)return true;
   }
  }
  return false;
 }
 var lastDir=1,synthetic=false,skipQueued=false;
 function sendNav(dir){
  if(synthetic)return;
  synthetic=true;
  var key=dir<0?'ArrowUp':'ArrowDown',code=dir<0?38:40;
  try{
   ['keydown','keyup'].forEach(function(type){
    var ev=new KeyboardEvent(type,{key:key,code:key,bubbles:true,cancelable:true});
    try{Object.defineProperty(ev,'keyCode',{get:function(){return code;}});}catch(x){}
    try{Object.defineProperty(ev,'which',{get:function(){return code;}});}catch(x){}
    document.dispatchEvent(ev);
   });
  }catch(x){}
  synthetic=false;
 }
 function skipIfNeeded(){
  skipQueued=false;
  if(!hiddenSelected())return;
  sendNav(lastDir);
  setTimeout(function(){if(hiddenSelected())sendNav(lastDir);},0);
 }
 function queueSkip(){if(skipQueued)return;skipQueued=true;setTimeout(skipIfNeeded,0);setTimeout(function(){if(hiddenSelected())sendNav(lastDir);},24);}
 document.addEventListener('keydown',function(e){
  if(synthetic)return;
  var k=String(e.key||'').toLowerCase(),c=e.keyCode||e.which||0;
  if(k==='arrowdown'||k==='s'||c===40){lastDir=1;setTimeout(queueSkip,0);}
  else if(k==='arrowup'||k==='w'||c===38){lastDir=-1;setTimeout(queueSkip,0);}
 },true);

 hideTargets();
 setTimeout(hideTargets,80);setTimeout(hideTargets,250);setTimeout(hideTargets,750);setTimeout(hideTargets,1500);
 var queued=false;
 var obs=new MutationObserver(function(){
  if(!queued){queued=true;setTimeout(function(){queued=false;hideTargets();queueSkip();},0);}
 });
 obs.observe(document.documentElement,{childList:true,subtree:true,characterData:true,attributes:true,
  attributeFilter:['class','role','aria-label','title','data-bind','data-action','data-event','data-command','onclick','aria-selected','data-selected','data-active','data-focused','tabindex']});
})();
)JS";

static const char kForceHudScript[] =
"if(window.__ControlDynamicHUDSuite&&window.__ControlDynamicHUDSuite.forceShowHUD)"
"window.__ControlDynamicHUDSuite.forceShowHUD();";

static const char kSuiteScript[] = R"JS(
(function(){
 if(window.__ControlDynamicHUDSuite)return;
 var CFG=window.__CDH_NATIVE_CONFIG||{
  diagnostics:0,showHudKey:'F1',showHudDurationMs:5000,
  healthEnabled:1,healthHideDelayMs:2000,healthFadeDurationMs:300,healthOpacityPercent:80,healthShowDuringCombat:1,healthThresholdPercent:100,
  missionEnabled:1,missionInitialHideDelayMs:2000,missionAfterMapCloseHideDelayMs:3000,missionUpdateVisibleMs:7000,missionFadeDurationMs:300,missionShowInMap:1,
  crosshairEnabled:1,crosshairHideDelayMs:1000,crosshairFadeDurationMs:300,
  crosshairDotEnabled:1,crosshairDotHideDelayMs:0,
  expeditionEnabled:1,expeditionHideDelayMs:2000,expeditionFadeDurationMs:300
 };
 var CDH=window.__ControlDynamicHUDSuite={
  version:'1.0S',health:false,mission:false,crosshair:false,expedition:false,
  hudVisible:true,active:true,lastError:'',forceVisible:false
 };
 var MODE={COMBAT:0,ADVENTURING:1,STORY:2,ACTION:3,EXAMINE:4,HIDDEN:5};
 var playerMode=1,isAiming=false,hudVisible=true,active=true;
 var hp={bar:null,fill:null,obs:null,timer:0,shown:true};
 var mission={map:null,log:null,obs:null,timer:0,shown:true};
 var cross={el:null,timer:0,shown:true,hideLatched:false};
 var dot={root:null,el:null,pseudo:'',timer:0,hidden:false,obs:null,marks:[]};
 var expedition={el:null,timer:0,shown:true};
 var resumeTimer=0,heartbeat=0,diagTimer=0,rebindTimer=0,forceTimer=0,forceUntil=0;

 function isForced(){return forceUntil>Date.now();}
 function addStyle(){
  if(document.getElementById('cdh-suite-style'))return;
  var s=document.createElement('style');s.id='cdh-suite-style';
  s.textContent=
   '.health-bar:not(.health-bar--hidden){transition:opacity '+CFG.healthFadeDurationMs+'ms var(--easing);}'
  +'.health-bar[data-cdh-hidden="0"]:not(.health-bar--hidden){opacity:'+(CFG.healthOpacityPercent/100)+';}'
  +'.health-bar[data-cdh-hidden="1"]:not(.health-bar--hidden){opacity:0;}'
  +'.health-bar--hidden{opacity:0!important;}'
  +'.mission-log-wrapper{transition:opacity '+CFG.missionFadeDurationMs+'ms var(--easing);}'
  +'.mission-log-wrapper.cdh-hide{opacity:0;}'
  +'.mission-log-wrapper.cdh-in-map{opacity:1!important;}'
  +'.awesome-crosshair{transition:opacity '+CFG.crosshairFadeDurationMs+'ms var(--easing);}'
  +'.awesome-crosshair[data-cdh-crosshair-hidden="1"]{opacity:0!important;}'
  +'.awesome-crosshair[data-cdh-dot-hidden="1"]{background-image:none!important;background-color:transparent!important;box-shadow:none!important;-webkit-mask-image:radial-gradient(circle at 50% 50%,transparent 0,transparent 0.28vmin,#000 0.34vmin,#000 100%)!important;-webkit-mask-repeat:no-repeat!important;-webkit-mask-size:100% 100%!important;mask-image:radial-gradient(circle at 50% 50%,transparent 0,transparent 0.28vmin,#000 0.34vmin,#000 100%)!important;mask-repeat:no-repeat!important;mask-size:100% 100%!important;}'
  +'.awesome-crosshair[data-cdh-dot-hidden="1"]::before,.awesome-crosshair[data-cdh-dot-hidden="1"]::after{content:none!important;background:none!important;box-shadow:none!important;}'
  +'.awesome-crosshair[data-cdh-dot-hidden="1"] .awesome-crosshair__dot,.awesome-crosshair[data-cdh-dot-hidden="1"] .awesome-crosshair-dot,.awesome-crosshair[data-cdh-dot-hidden="1"] .crosshair-dot,.awesome-crosshair[data-cdh-dot-hidden="1"] [class*="crosshair"][class*="dot"],.awesome-crosshair[data-cdh-dot-hidden="1"] [class*="reticul"][class*="dot"]{opacity:0!important;visibility:hidden!important;}'
  +'[data-cdh-center-dot-hidden="1"]{display:none!important;opacity:0!important;visibility:hidden!important;}'
  +'[data-cdh-dot-pseudo-before="1"]::before,[data-cdh-dot-pseudo-after="1"]::after{display:none!important;content:none!important;opacity:0!important;visibility:hidden!important;}'
  +'.expedition-hud>.expedition-mod-group{transition:opacity '+CFG.expeditionFadeDurationMs+'ms var(--easing);}'
  +'.expedition-hud>.expedition-mod-group[data-cdh-expedition-hidden="1"]{opacity:0!important;}'
  +'#cdh-diagnostic{position:absolute;left:18px;top:18px;z-index:2147483647;'
  +'font:15px monospace;color:white;background:rgba(0,0,0,.72);padding:10px 12px;'
  +'pointer-events:none;white-space:pre;line-height:1.25;max-width:760px;}';
  (document.head||document.documentElement).appendChild(s);
 }

 function clearTimer(o){if(o.timer){clearTimeout(o.timer);o.timer=0;}}
 function disconnectObserver(o){if(o.obs){o.obs.disconnect();o.obs=null;}}
 function setActive(v){
  if(v===active)return;
  active=v;CDH.active=v;
  if(v)resumeHud();else suspendHud();
 }
 function recomputeActive(){
  hudVisible=(g_HUDMode.m_bIsHudVisible!==false);
  CDH.hudVisible=hudVisible;
  setActive(hudVisible&&playerMode!==MODE.HIDDEN);
 }

 function hpPercent(){
  if(!hp.fill)return 1;
  var ow=hp.fill.offsetWidth;
  return ow?hp.fill.getBoundingClientRect().width/ow:1;
 }
 function setHpHidden(v){
  if(!active||!hp.bar||!hp.bar.isConnected)return;
  hp.bar.setAttribute('data-cdh-hidden',v?'1':'0');hp.shown=!v;
 }
 function updateHealth(){
  if(!active||!CFG.healthEnabled)return;
  if(!hp.bar||!hp.fill||!hp.bar.isConnected||!hp.fill.isConnected){
   CDH.health=false;scheduleRebind(50);return;
  }
  if(isForced()){clearTimer(hp);setHpHidden(false);return;}
  var threshold=CFG.healthThresholdPercent/100;
  var should=(hpPercent()<threshold)||(CFG.healthShowDuringCombat&&playerMode===MODE.COMBAT);
  if(should){clearTimer(hp);if(!hp.shown)setHpHidden(false);}
  else if(hp.shown&&!hp.timer){
   hp.timer=setTimeout(function(){
    hp.timer=0;
    if(active&&!isForced()&&hp.bar&&hp.bar.isConnected)setHpHidden(true);
   },CFG.healthHideDelayMs);
  }
 }
 function bindHealth(){
  if(!active||!CFG.healthEnabled)return false;
  if(window.g_runtimeInterfaceOptions&&g_runtimeInterfaceOptions.m_bPlayerStatsEnabled===false)return false;
  var b=document.querySelector('.health-bar'),f=document.querySelector('.health-bar__fill');
  if(!b||!f){CDH.health=false;return false;}
  if(b!==hp.bar||f!==hp.fill||!hp.obs){
   clearTimer(hp);disconnectObserver(hp);
   hp.bar=b;hp.fill=f;hp.shown=true;
   hp.obs=new MutationObserver(updateHealth);
   hp.obs.observe(f,{attributes:true,attributeFilter:['style']});
  }
  CDH.health=true;updateHealth();return true;
 }

 function missionHide(ms){
  if(!active||!CFG.missionEnabled||!mission.log)return;
  clearTimer(mission);
  if(isForced()){missionShow();return;}
  mission.timer=setTimeout(function(){
   mission.timer=0;
   if(!active||isForced())return;
   mission.shown=false;
   if(mission.log&&mission.log.isConnected)mission.log.classList.add('cdh-hide');
  },ms);
 }
 function missionShow(){
  if(!active||!mission.log||!mission.log.isConnected)return;
  clearTimer(mission);mission.shown=true;mission.log.classList.remove('cdh-hide');
 }
 function onMapChanged(){
  if(!active||!CFG.missionEnabled)return;
  if(!mission.map||!mission.log||!mission.map.isConnected||!mission.log.isConnected){
   CDH.mission=false;scheduleRebind(50);return;
  }
  if(isForced()){missionShow();return;}
  var inMap=mission.map.classList.contains('map--show');
  mission.log.classList.toggle('cdh-in-map',CFG.missionShowInMap&&inMap);
  if(CFG.missionShowInMap&&inMap)missionShow();
  else if(mission.shown)missionHide(CFG.missionAfterMapCloseHideDelayMs);
 }
 function bindMission(){
  if(!active||!CFG.missionEnabled)return false;
  if(window.g_runtimeInterfaceOptions&&g_runtimeInterfaceOptions.m_bMissionHUDEnabled===false)return false;
  var m=document.querySelector('.map-overlay'),l=document.querySelector('.mission-log-wrapper');
  if(!m||!l){CDH.mission=false;return false;}
  if(m!==mission.map||l!==mission.log||!mission.obs){
   clearTimer(mission);disconnectObserver(mission);
   mission.map=m;mission.log=l;mission.shown=true;
   mission.log.classList.remove('cdh-hide');
   mission.obs=new MutationObserver(onMapChanged);
   mission.obs.observe(m,{attributes:true,attributeFilter:['class']});
  }
  CDH.mission=true;onMapChanged();
  if(!isForced()&&!m.classList.contains('map--show'))missionHide(CFG.missionInitialHideDelayMs);
  return true;
 }

 function setCrossHidden(v){
  if(!cross.el||!cross.el.isConnected)return;
  cross.el.setAttribute('data-cdh-crosshair-hidden',v?'1':'0');cross.shown=!v;
 }
 function crosshairShouldShow(){
  return playerMode===MODE.COMBAT||isAiming;
 }
 function updateCrosshair(){
  if(!active||!CFG.crosshairEnabled)return;
  if(!cross.el||!cross.el.isConnected){CDH.crosshair=false;scheduleRebind(100);return;}
  if(isForced()){clearTimer(cross);setCrossHidden(false);return;}

  if(crosshairShouldShow()){
   clearTimer(cross);
   cross.hideLatched=false;
   setCrossHidden(false);
   return;
  }

  if(cross.hideLatched){
   clearTimer(cross);
   setCrossHidden(true);
   return;
  }

  if(cross.shown&&!cross.timer){
   cross.timer=setTimeout(function(){
    cross.timer=0;
    if(active&&!isForced()&&!crosshairShouldShow()){
     cross.hideLatched=true;
     setCrossHidden(true);
    }
   },CFG.crosshairHideDelayMs);
  }
 }
 function bindCrosshair(){
  if(!active||!CFG.crosshairEnabled)return false;
  var e=document.querySelector('.awesome-crosshair');
  if(!e){cross.el=null;CDH.crosshair=false;return false;}
  var changed=(e!==cross.el);
  cross.el=e;cross.shown=e.getAttribute('data-cdh-crosshair-hidden')!=='1';CDH.crosshair=true;
  if(changed&&cross.hideLatched&&!crosshairShouldShow()&&!isForced())setCrossHidden(true);
  else updateCrosshair();
  return true;
 }


 function clearDotVisualMarks(){
  if(dot.root)dot.root.setAttribute('data-cdh-dot-hidden','0');
  var marks=dot.marks||[];
  for(var i=0;i<marks.length;i++){
   var e=marks[i]&&marks[i].el;if(!e)continue;
   try{
    e.removeAttribute('data-cdh-center-dot-hidden');
    e.removeAttribute('data-cdh-dot-pseudo-before');
    e.removeAttribute('data-cdh-dot-pseudo-after');
   }catch(x){}
  }
  dot.marks=[];
  dot.el=null;dot.pseudo='';
 }
 function clearDotMark(){
  if(dot.obs){try{dot.obs.disconnect();}catch(x){}dot.obs=null;}
  clearDotVisualMarks();
 }
 function isDotLikeRect(e,cx,cy,maxSize,maxDelta){
  if(!e||e===document.body||e===document.documentElement)return false;
  var r;try{r=e.getBoundingClientRect();}catch(x){return false;}
  var w=r.width,h=r.height;
  if(!(w>=0.25&&h>=0.25&&w<=maxSize&&h<=maxSize))return false;
  var ratio=w>h?w/h:h/w;
  if(ratio>1.85)return false;
  var dx=Math.abs((r.left+w*0.5)-cx),dy=Math.abs((r.top+h*0.5)-cy);
  if(dx>maxDelta||dy>maxDelta)return false;
  var cs;try{cs=window.getComputedStyle(e);}catch(x2){cs=null;}
  if(cs&&(cs.display==='none'||cs.visibility==='hidden'||parseFloat(cs.opacity||'1')<0.02))return false;
  var sg=(String(e.className||'')+' '+String(e.id||'')+' '+String(e.tagName||'')).toLowerCase();
  if(/ammo|target|enemy|lock|charge|hit|damage|prompt|health|mission|interaction/.test(sg))return false;
  return true;
 }
 function collectCenterDots(root){
  var out=[];
  function add(e,pseudo){
   if(!e||e===root&&pseudo==='')return;
   pseudo=pseudo||'';
   for(var k=0;k<out.length;k++)if(out[k].el===e&&out[k].pseudo===pseudo)return;
   out.push({el:e,pseudo:pseudo});
  }
  var rr=root.getBoundingClientRect(),cx=window.innerWidth*0.5,cy=window.innerHeight*0.5;
  if(rr&&rr.width>2&&rr.height>2){cx=rr.left+rr.width*0.5;cy=rr.top+rr.height*0.5;}

  var sels=['.awesome-crosshair__dot','.awesome-crosshair-dot','.crosshair-dot',
   '[class*="crosshair"][class*="dot"]','[class*="reticul"][class*="dot"]',
   '[class*="reticle"][class*="dot"]','[id*="crosshair"][id*="dot"]',
   '[class*="crosshair"][class*="center"]','[class*="reticul"][class*="center"]'];
  for(var si=0;si<sels.length;si++){
   try{var sq=root.querySelectorAll(sels[si]);for(var sj=0;sj<sq.length;sj++)add(sq[sj],'');}catch(x){}
   try{var gq=document.querySelectorAll(sels[si]);for(var gj=0;gj<gq.length;gj++)if(isDotLikeRect(gq[gj],cx,cy,32,10))add(gq[gj],'');}catch(x2){}
  }

  var all=root.querySelectorAll('*');
  for(var i=0;i<all.length&&i<512;i++){
   var e=all[i];
   if(isDotLikeRect(e,cx,cy,24,8))add(e,'');
  }

  if(document.elementsFromPoint){
   var stack=[];try{stack=document.elementsFromPoint(cx,cy)||[];}catch(x3){}
   for(var st=0;st<stack.length;st++)
    if(isDotLikeRect(stack[st],cx,cy,24,6))add(stack[st],'');
  }

  var svg=document.querySelectorAll('svg circle,svg ellipse,svg rect,svg path,[class*="dot"],[id*="dot"],[class*="center"],[id*="center"]');
  for(var v=0;v<svg.length&&v<512;v++)
   if(isDotLikeRect(svg[v],cx,cy,24,6))add(svg[v],'');

  var nodes=[root];
  for(var n=0;n<all.length&&n<192;n++)nodes.push(all[n]);
  for(var ni=0;ni<nodes.length;ni++){
   var pe=nodes[ni],sg=(String(pe.className||'')+' '+String(pe.id||'')).toLowerCase();
   for(var pi=0;pi<2;pi++){
    var ps=pi===0?'::before':'::after',cs;try{cs=window.getComputedStyle(pe,ps);}catch(x4){cs=null;}
    if(!cs||cs.display==='none'||cs.visibility==='hidden'||parseFloat(cs.opacity||'1')<0.02)continue;
    var w=parseFloat(cs.width),h=parseFloat(cs.height),ratio=(w>h?w/h:h/w);
    if(!(w>=0.25&&h>=0.25&&w<=24&&h<=24&&ratio<=1.85))continue;
    var pos=(String(cs.left||'')+' '+String(cs.top||'')+' '+String(cs.transform||'')).toLowerCase();
    if(pe===root||/dot|center|centre/.test(sg)||pos.indexOf('50%')>=0)
      add(pe,pi===0?'before':'after');
   }
  }
  return out;
 }
 function applyDotMarks(v){
  if(!dot.root||!dot.root.isConnected)return;
  dot.root.setAttribute('data-cdh-dot-hidden',v?'1':'0');
  var marks=dot.marks||[];
  for(var i=0;i<marks.length;i++){
   var m=marks[i],e=m&&m.el;if(!e)continue;
   try{
    if(m.pseudo==='before')e.setAttribute('data-cdh-dot-pseudo-before',v?'1':'0');
    else if(m.pseudo==='after')e.setAttribute('data-cdh-dot-pseudo-after',v?'1':'0');
    else e.setAttribute('data-cdh-center-dot-hidden',v?'1':'0');
   }catch(x){}
  }
  dot.hidden=v;
 }
 function setDotHidden(v){applyDotMarks(v);}
 function scheduleCrosshairDotHide(){
  clearTimer(dot);
  if(!active||!CFG.crosshairDotEnabled||!dot.root)return;
  if(isForced()){setDotHidden(false);return;}
  if(CFG.crosshairDotHideDelayMs===0){setDotHidden(true);return;}
  setDotHidden(false);
  dot.timer=setTimeout(function(){
   dot.timer=0;
   if(active&&!isForced()&&dot.root&&dot.root.isConnected)setDotHidden(true);
  },CFG.crosshairDotHideDelayMs);
 }
 function bindCrosshairDot(){
  if(!active||!CFG.crosshairDotEnabled)return false;
  var root=document.querySelector('.awesome-crosshair');
  if(!root){
   clearTimer(dot);clearDotMark();dot.root=null;dot.hidden=false;CDH.crosshairDot=false;return false;
  }
  var rootChanged=root!==dot.root;
  if(rootChanged){
   clearTimer(dot);clearDotMark();dot.root=root;dot.hidden=false;
   try{
    dot.obs=new MutationObserver(function(){if(active&&CFG.crosshairDotEnabled)scheduleRebind(0);});
    dot.obs.observe(root,{childList:true,subtree:true,attributes:true,attributeFilter:['class','style','transform']});
   }catch(x){}
  }else if(dot.hidden){
   // Temporarily reveal previous marks while rescanning so stacked dots are not hidden from geometry tests.
   applyDotMarks(false);
  }
  dot.marks=collectCenterDots(root);
  if(dot.marks.length){dot.el=dot.marks[0].el;dot.pseudo=dot.marks[0].pseudo||'';}
  else {dot.el=null;dot.pseudo='';}
  CDH.crosshairDot=true;
  if(isForced())setDotHidden(false);
  else scheduleCrosshairDotHide();
  return true;
 }

 function setExpeditionHidden(v){
  if(!expedition.el||!expedition.el.isConnected)return;
  expedition.el.setAttribute('data-cdh-expedition-hidden',v?'1':'0');expedition.shown=!v;
 }
 function scheduleExpeditionHide(){
  if(!active||!CFG.expeditionEnabled||!expedition.el)return;
  clearTimer(expedition);
  if(isForced()){setExpeditionHidden(false);return;}
  expedition.timer=setTimeout(function(){
   expedition.timer=0;
   if(active&&!isForced())setExpeditionHidden(true);
  },CFG.expeditionHideDelayMs);
 }
 function bindExpedition(){
  if(!active||!CFG.expeditionEnabled)return false;
  var e=document.querySelector('.expedition-hud > .expedition-mod-group');
  if(!e){expedition.el=null;CDH.expedition=false;return false;}
  if(e!==expedition.el){
   clearTimer(expedition);
   expedition.el=e;expedition.shown=true;
   setExpeditionHidden(false);
   scheduleExpeditionHide();
  }else if(isForced()){
   setExpeditionHidden(false);
  }
  CDH.expedition=true;
  return true;
 }


 function rebind(){
  rebindTimer=0;
  if(!active)return;
  addStyle();
  bindHealth();bindMission();bindCrosshair();bindCrosshairDot();bindExpedition();updateDiag();
 }
 function scheduleRebind(ms){
  if(!active||rebindTimer)return;
  rebindTimer=setTimeout(rebind,ms||0);
 }
 function rebindBurst(){
  if(!active)return;
  var delays=[0,100,250,500,1000];
  for(var i=0;i<delays.length;i++){
   (function(d){setTimeout(function(){if(active)rebind();},d);})(delays[i]);
  }
 }

 function suspendHud(){
  if(resumeTimer){clearTimeout(resumeTimer);resumeTimer=0;}
  if(rebindTimer){clearTimeout(rebindTimer);rebindTimer=0;}
  clearTimer(hp);clearTimer(mission);clearTimer(cross);clearTimer(dot);clearTimer(expedition);
  disconnectObserver(hp);disconnectObserver(mission);
  clearDotMark();hp.bar=null;hp.fill=null;mission.map=null;mission.log=null;cross.el=null;dot.root=null;dot.el=null;dot.pseudo='';dot.hidden=false;expedition.el=null;
  CDH.health=false;CDH.mission=false;CDH.crosshair=false;CDH.crosshairDot=false;CDH.expedition=false;
  updateDiag();
 }
 function resumeHud(){
  if(resumeTimer)clearTimeout(resumeTimer);
  resumeTimer=setTimeout(function(){
   resumeTimer=0;
   if(!active)return;
   rebindBurst();
  },150);
 }

 function heartbeatTick(){
  if(!active)return;
  var needs=
   (CFG.healthEnabled&&(!hp.bar||!hp.fill||!hp.bar.isConnected||!hp.fill.isConnected))||
   (CFG.missionEnabled&&(!mission.map||!mission.log||!mission.map.isConnected||!mission.log.isConnected))||
   (CFG.crosshairEnabled&&(!cross.el||!cross.el.isConnected))||
   (CFG.crosshairDotEnabled&&(!dot.root||!dot.root.isConnected))||
   (CFG.expeditionEnabled&&(!expedition.el||!expedition.el.isConnected));
  if(needs)scheduleRebind(0);
 }

 function forceShowHUD(){
  if(!active)return;
  forceUntil=Date.now()+CFG.showHudDurationMs;CDH.forceVisible=true;
  clearTimer(hp);clearTimer(mission);clearTimer(cross);clearTimer(dot);clearTimer(expedition);
  if(hp.bar)setHpHidden(false);
  if(mission.log)missionShow();
  if(cross.el){setCrossHidden(false);}
  if(dot.root)setDotHidden(false);
  if(expedition.el)setExpeditionHidden(false);
  if(forceTimer)clearTimeout(forceTimer);
  forceTimer=setTimeout(function(){
   forceTimer=0;forceUntil=0;CDH.forceVisible=false;
   if(!active)return;
   cross.hideLatched=false;
   updateHealth();onMapChanged();updateCrosshair();bindCrosshairDot();scheduleCrosshairDotHide();scheduleExpeditionHide();
  },CFG.showHudDurationMs);
  updateDiag();
 }
 CDH.forceShowHUD=forceShowHUD;

 function yes(v){return v?'YES':'no';}
 function updateDiag(){
  if(!CFG.diagnostics)return;
  var p=document.getElementById('cdh-diagnostic');if(!p)return;
  p.textContent=
   'Control Dynamic HUD v1.0P configurable suite\n'
  +'HUD '+yes(hudVisible)+' | active '+yes(active)+' | force '+yes(isForced())+' | aim '+yes(isAiming)+' | key '+CFG.showHudKey+'\n'
  +'health '+yes(CDH.health)+' | mission '+yes(CDH.mission)+' | crosshair '+yes(CDH.crosshair)+' | dot '+yes(CDH.crosshairDot)+' | expedition '+yes(CDH.expedition)+'\n'
  +'ammo '+yes(!!document.querySelector('.awesome-crosshair--ammo'))+' | enemyHP '+yes(!!document.querySelector('.enemy-health-container'))+' | energy '+yes(!!document.querySelector('.ability-resource-bar'))+'\n'
  +'pickup '+yes(!!document.querySelector('.pickup-notifications-container'))+' | interaction '+yes(!!document.querySelector('.interaction-marker-container'))+' | threat '+yes(!!document.querySelector('.threat-indicators'))+
   (CDH.lastError?'\nERR '+CDH.lastError:'');
 }
 function startDiag(){
  if(!CFG.diagnostics||document.getElementById('cdh-diagnostic'))return;
  var p=document.createElement('div');p.id='cdh-diagnostic';
  (document.body||document.documentElement).appendChild(p);updateDiag();
  diagTimer=setInterval(updateDiag,500);
 }
 window.addEventListener('error',function(e){
  CDH.lastError=(e&&e.message)?String(e.message):'window error';updateDiag();
 });
 window.addEventListener('unhandledrejection',function(e){
  CDH.lastError='promise: '+String(e&&e.reason?e.reason:'unknown');updateDiag();
 });

 var attempts=0;
 function boot(){
  if(!document.documentElement||!window.engine||!window.g_HUDMode){
   if(++attempts<100)setTimeout(boot,100);
   return;
  }
  playerMode=g_HUDMode.m_iPlayerMode;
  hudVisible=(g_HUDMode.m_bIsHudVisible!==false);
  active=hudVisible&&playerMode!==MODE.HIDDEN;
  CDH.hudVisible=hudVisible;CDH.active=active;

  engine.addModelChangeListener(g_HUDMode,'m_iPlayerMode',function(){
   playerMode=g_HUDMode.m_iPlayerMode;recomputeActive();
   if(active){updateHealth();updateCrosshair();scheduleRebind(0);}updateDiag();
  });
  engine.addModelChangeListener(g_HUDMode,'m_bIsHudVisible',function(){
   recomputeActive();updateDiag();
  });
  engine.on('onPlayerAimChanged',function(v){
   isAiming=!!v;
   if(active){updateCrosshair();}
   updateDiag();
  });
  if(window.g_missionPromptUIData){
   engine.addModelChangeListener(g_missionPromptUIData,'m_missionUIData',function(){
    if(active&&CFG.missionEnabled&&mission.log){
     missionShow();
     if(CFG.missionUpdateVisibleMs>0)missionHide(CFG.missionUpdateVisibleMs);
    }
    updateDiag();
   });
  }
  startDiag();
  heartbeat=setInterval(heartbeatTick,1000);
  if(active)rebindBurst();
 }
 boot();
})();
)JS";

static inline void* get_peb() { void* peb; __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(peb)); return peb; }
static u8 rd8(const void* p){ return *(const volatile u8*)p; }
static u16 rd16(const void* p){ return *(const volatile u16*)p; }
static u32 rd32(const void* p){ return *(const volatile u32*)p; }
static uptr rdptr(const void* p){ return *(const volatile uptr*)p; }
static char lower_ascii(char c){ return (c>='A'&&c<='Z')?char(c+32):c; }
static wchar_t lower_w(wchar_t c){ return (c>=L'A'&&c<=L'Z')?wchar_t(c+32):c; }
static bool streq_ascii(const char* a,const char* b){ while(*a&&*b){if(*a!=*b)return false;++a;++b;}return *a==*b; }

static bool module_name_eq(const UNICODE_STRING_T* us,const char* ascii){
    if(!us||!us->Buffer)return false; usize n=us->Length/2,m=0; while(ascii[m])++m; if(n!=m)return false;
    for(usize i=0;i<n;++i) if(lower_w(us->Buffer[i])!=(wchar_t)(unsigned char)lower_ascii(ascii[i])) return false; return true;
}
static void* find_module(const char* baseName){
    u8* peb=(u8*)get_peb(); if(!peb)return nullptr; u8* ldr=(u8*)rdptr(peb+0x18); if(!ldr)return nullptr;
    LIST_ENTRY_T* head=(LIST_ENTRY_T*)(ldr+0x20); for(LIST_ENTRY_T* e=head->Flink;e&&e!=head;e=e->Flink){
        u8* entry=(u8*)e-0x10; auto* bn=(UNICODE_STRING_T*)(entry+0x58); if(module_name_eq(bn,baseName)) return (void*)rdptr(entry+0x30);
    } return nullptr;
}
static void* first_module_base(){
    u8* peb=(u8*)get_peb(); if(!peb)return nullptr; u8* ldr=(u8*)rdptr(peb+0x18); if(!ldr)return nullptr;
    auto* head=(LIST_ENTRY_T*)(ldr+0x20); auto* e=head->Flink; if(!e||e==head)return nullptr; return (void*)rdptr((u8*)e-0x10+0x30);
}
static const UNICODE_STRING_T* first_module_name(){
    u8* peb=(u8*)get_peb(); if(!peb)return nullptr; u8* ldr=(u8*)rdptr(peb+0x18); if(!ldr)return nullptr;
    auto* head=(LIST_ENTRY_T*)(ldr+0x20); auto* e=head->Flink; if(!e||e==head)return nullptr; return (const UNICODE_STRING_T*)((u8*)e-0x10+0x58);
}

static void* resolve_export(void* module,const char* name,int depth=0);
static void* resolve_forwarder(const char* fwd,int depth){
    if(depth>4)return nullptr; char modn[64],sym[128]; usize i=0;
    while(fwd[i]&&fwd[i]!='.'&&i<55){modn[i]=lower_ascii(fwd[i]);++i;} if(fwd[i]!='.')return nullptr; modn[i++]=0;
    usize ml=0; while(modn[ml])++ml; if(ml<4||!(modn[ml-4]=='.'&&modn[ml-3]=='d'&&modn[ml-2]=='l'&&modn[ml-1]=='l')){modn[ml++]='.';modn[ml++]='d';modn[ml++]='l';modn[ml++]='l';modn[ml]=0;}
    usize j=0; while(fwd[i]&&j<127)sym[j++]=fwd[i++]; sym[j]=0; if(sym[0]=='#')return nullptr; void* m=find_module(modn); return m?resolve_export(m,sym,depth+1):nullptr;
}
static void* resolve_export(void* module,const char* name,int depth){
    if(!module||depth>4)return nullptr; u8* base=(u8*)module; if(rd16(base)!=0x5A4D)return nullptr; u32 e=rd32(base+0x3C); u8* nt=base+e; if(rd32(nt)!=0x4550)return nullptr;
    u8* opt=nt+24; if(rd16(opt)!=0x20B)return nullptr; u32 er=rd32(opt+0x70),es=rd32(opt+0x74); if(!er||!es)return nullptr; u8* ex=base+er;
    u32 nn=rd32(ex+0x18),fr=rd32(ex+0x1C),nr=rd32(ex+0x20),orr=rd32(ex+0x24); u32* funcs=(u32*)(base+fr);u32* names=(u32*)(base+nr);u16* ords=(u16*)(base+orr);
    for(u32 i=0;i<nn;++i){const char* n=(const char*)(base+names[i]); if(!streq_ascii(n,name))continue; u32 r=funcs[ords[i]]; if(r>=er&&r<er+es)return resolve_forwarder((const char*)(base+r),depth); return base+r;} return nullptr;
}

struct WinApi { FnCreateFileW CreateFileW; FnWriteFile WriteFile; FnCloseHandle CloseHandle; FnVirtualAlloc VirtualAlloc; FnVirtualProtect VirtualProtect; FnFlushInstructionCache FlushInstructionCache; FnGetPrivateProfileIntA GetPrivateProfileIntA; FnGetPrivateProfileStringA GetPrivateProfileStringA; FnGetAsyncKeyState GetAsyncKeyState; FnGetTickCount64 GetTickCount64; };
static bool init_api(WinApi& a){
    void* k=find_module("kernel32.dll"); if(!k)return false;
    a.CreateFileW=(FnCreateFileW)resolve_export(k,"CreateFileW"); a.WriteFile=(FnWriteFile)resolve_export(k,"WriteFile"); a.CloseHandle=(FnCloseHandle)resolve_export(k,"CloseHandle");
    a.VirtualAlloc=(FnVirtualAlloc)resolve_export(k,"VirtualAlloc"); a.VirtualProtect=(FnVirtualProtect)resolve_export(k,"VirtualProtect"); a.FlushInstructionCache=(FnFlushInstructionCache)resolve_export(k,"FlushInstructionCache");
    a.GetPrivateProfileIntA=(FnGetPrivateProfileIntA)resolve_export(k,"GetPrivateProfileIntA");
    a.GetPrivateProfileStringA=(FnGetPrivateProfileStringA)resolve_export(k,"GetPrivateProfileStringA");
    a.GetTickCount64=(FnGetTickCount64)resolve_export(k,"GetTickCount64");
    void* u=find_module("user32.dll");
    a.GetAsyncKeyState=u?(FnGetAsyncKeyState)resolve_export(u,"GetAsyncKeyState"):nullptr;
    return a.CreateFileW&&a.WriteFile&&a.CloseHandle&&a.VirtualAlloc&&a.VirtualProtect&&a.FlushInstructionCache&&a.GetPrivateProfileIntA&&a.GetPrivateProfileStringA;
}

static void log_bytes(const char* s,DWORD n){ if(!g_log||!g_WriteFile)return; DWORD w=0; g_WriteFile(g_log,s,n,&w,nullptr); }
static void log_cstr(const char* s){ DWORD n=0;while(s[n])++n;log_bytes(s,n); }
static void log_line(const char* s){log_cstr(s);log_bytes("\r\n",2);}
static void log_hex(uptr v){char b[18];b[0]='0';b[1]='x';const char*h="0123456789ABCDEF";for(int i=0;i<16;++i)b[2+i]=h[(v>>((15-i)*4))&15];log_bytes(b,18);}
static void log_dec(u64 v){char b[32];int p=31;b[p--]=0;if(!v)b[p--]='0';while(v&&p>=0){b[p--]=char('0'+v%10);v/=10;}log_cstr(b+p+1);}
static void log_wide(const UNICODE_STRING_T* us){if(!us||!us->Buffer){log_cstr("<unknown>");return;}char b[260];usize n=us->Length/2,o=0;for(usize i=0;i<n&&o<259;++i){wchar_t c=us->Buffer[i];b[o++]=(c>=32&&c<127)?char(c):'?';}b[o]=0;log_cstr(b);}

static bool coherent_build_matches(void* module){
    if(!module)return false;
    u8* base=(u8*)module;
    if(rd16(base)!=0x5A4D)return false;
    u32 e=rd32(base+0x3C);
    u8* nt=base+e;
    if(rd32(nt)!=0x4550)return false;
    u32 timestamp=rd32(nt+8);
    u8* opt=nt+24;
    if(rd16(opt)!=0x20B)return false;
    u32 imageSize=rd32(opt+0x38);
    u32 checksum=rd32(opt+0x40);
    log_cstr("Coherent fingerprint: TimeDateStamp=");log_hex(timestamp);
    log_cstr(" SizeOfImage=");log_hex(imageSize);
    log_cstr(" CheckSum=");log_hex(checksum);log_bytes("\r\n",2);
    return timestamp==kCoherentTimeDateStamp&&imageSize==kCoherentSizeOfImage&&checksum==kCoherentCheckSum;
}



static u32 clamp_u32(u32 v,u32 lo,u32 hi){return v<lo?lo:(v>hi?hi:v);}
static void copy_key(char* dst,usize cap,const char* src){
    if(!dst||!cap)return;usize o=0;
    while(src&&src[o]&&o+1<cap){
        char c=src[o];
        bool ok=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9');
        dst[o]=ok?c:'_';++o;
    }
    dst[o]=0;
}
static u32 parse_vk(const char* s){
    if(!s||!s[0])return 0;
    char b[16]{};usize n=0;
    while(s[n]&&n<15){char c=s[n];b[n]=(c>='a'&&c<='z')?char(c-32):c;++n;}b[n]=0;
    if(n==1){
        if((b[0]>='A'&&b[0]<='Z')||(b[0]>='0'&&b[0]<='9'))return (u32)b[0];
    }
    if(b[0]=='F'&&n>=2&&n<=3){
        u32 v=0;for(usize i=1;i<n;++i){if(b[i]<'0'||b[i]>'9')return 0;v=v*10+(u32)(b[i]-'0');}
        if(v>=1&&v<=12)return 0x70u+(v-1);
    }
    if(streq_ascii(b,"INSERT"))return 0x2D;
    if(streq_ascii(b,"HOME"))return 0x24;
    if(streq_ascii(b,"END"))return 0x23;
    if(streq_ascii(b,"PAGEUP"))return 0x21;
    if(streq_ascii(b,"PAGEDOWN"))return 0x22;
    if(streq_ascii(b,"DELETE"))return 0x2E;
    return 0;
}
static char* app(char* p,char* end,const char* s){while(*s&&p<end-1)*p++=*s++;*p=0;return p;}
static char* app_u32(char* p,char* end,u32 v){
    char b[16];int n=0;if(!v)b[n++]='0';else{char r[16];int k=0;while(v&&k<16){r[k++]=char('0'+v%10);v/=10;}while(k)b[n++]=r[--k];}
    b[n]=0;return app(p,end,b);
}
static void load_config(WinApi& a){
    const char* ini="plugins\\ControlDynamicHUD.ini";
    g_cfg.enabled=a.GetPrivateProfileIntA("General","Enabled",1,ini)!=0;
    g_cfg.diagnostics=a.GetPrivateProfileIntA("General","ShowDiagnostics",0,ini)!=0;
    char key[16]{};a.GetPrivateProfileStringA("Hotkeys","ShowHUDKey","F1",key,sizeof(key),ini);copy_key(g_cfg.showHudKey,sizeof(g_cfg.showHudKey),key);
    g_cfg.showHudDurationMs=clamp_u32(a.GetPrivateProfileIntA("Hotkeys","ShowHUDDurationMs",5000,ini),0,60000);

    g_cfg.healthEnabled=a.GetPrivateProfileIntA("Health","Enabled",1,ini)!=0;
    g_cfg.healthHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("Health","HideDelayMs",2000,ini),0,60000);
    g_cfg.healthFadeDurationMs=clamp_u32(a.GetPrivateProfileIntA("Health","FadeDurationMs",300,ini),0,10000);
    g_cfg.healthOpacityPercent=clamp_u32(a.GetPrivateProfileIntA("Health","VisibleOpacityPercent",80,ini),0,100);
    g_cfg.healthShowDuringCombat=a.GetPrivateProfileIntA("Health","ShowDuringCombat",1,ini)!=0;
    g_cfg.healthThresholdPercent=clamp_u32(a.GetPrivateProfileIntA("Health","ShowHealthThresholdPercent",100,ini),0,100);

    g_cfg.missionEnabled=a.GetPrivateProfileIntA("MissionLog","Enabled",1,ini)!=0;
    g_cfg.missionInitialHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("MissionLog","InitialHideDelayMs",2000,ini),0,60000);
    g_cfg.missionAfterMapCloseHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("MissionLog","AfterMapCloseHideDelayMs",3000,ini),0,60000);
    g_cfg.missionUpdateVisibleMs=clamp_u32(a.GetPrivateProfileIntA("MissionLog","MissionUpdateVisibleMs",7000,ini),0,60000);
    g_cfg.missionFadeDurationMs=clamp_u32(a.GetPrivateProfileIntA("MissionLog","FadeDurationMs",300,ini),0,10000);
    g_cfg.missionShowInMap=a.GetPrivateProfileIntA("MissionLog","ShowInMap",1,ini)!=0;

    g_cfg.crosshairEnabled=a.GetPrivateProfileIntA("Crosshair","Enabled",1,ini)!=0;
    g_cfg.crosshairHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("Crosshair","HideDelayMs",1000,ini),0,60000);
    g_cfg.crosshairFadeDurationMs=clamp_u32(a.GetPrivateProfileIntA("Crosshair","FadeDurationMs",300,ini),0,10000);

    g_cfg.crosshairDotEnabled=a.GetPrivateProfileIntA("CrosshairDot","Enabled",1,ini)!=0;
    g_cfg.crosshairDotHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("CrosshairDot","HideDelayMs",0,ini),0,60000);
    g_cfg.expeditionEnabled=a.GetPrivateProfileIntA("Expedition","Enabled",1,ini)!=0;
    g_cfg.expeditionHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("Expedition","HideDelayMs",2000,ini),0,60000);
    g_cfg.expeditionFadeDurationMs=clamp_u32(a.GetPrivateProfileIntA("Expedition","FadeDurationMs",300,ini),0,10000);

    g_cfg.hideGroundSlamTargetCircle=a.GetPrivateProfileIntA("GroundSlam","HideTargetCircle",1,ini)!=0;

    g_cfg.launchHideReticle=a.GetPrivateProfileIntA("Launch","HideTargetReticle",1,ini)!=0;
    g_cfg.launchHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("Launch","HideDelayMs",0,ini),0,60000);

    g_cfg.multiLaunchHideObjectIndicators=a.GetPrivateProfileIntA("MultiLaunch","HideObjectIndicators",1,ini)!=0;
    g_cfg.multiLaunchHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("MultiLaunch","HideDelayMs",0,ini),0,60000);
    g_cfg.mainMenuHideNewGame=a.GetPrivateProfileIntA("MainMenu","HideNewGame",0,ini)!=0;
    g_cfg.mainMenuHideMissionSelect=a.GetPrivateProfileIntA("MainMenu","HideMissionSelect",0,ini)!=0;
}
static void build_config_script(){
    char* p=g_configScript;char* end=g_configScript+sizeof(g_configScript);
    p=app(p,end,"window.__CDH_NATIVE_CONFIG={diagnostics:");p=app_u32(p,end,g_cfg.diagnostics?1:0);
    p=app(p,end,",showHudKey:'");p=app(p,end,g_cfg.showHudKey);p=app(p,end,"',showHudDurationMs:");p=app_u32(p,end,g_cfg.showHudDurationMs);
    p=app(p,end,",healthEnabled:");p=app_u32(p,end,g_cfg.healthEnabled?1:0);
    p=app(p,end,",healthHideDelayMs:");p=app_u32(p,end,g_cfg.healthHideDelayMs);
    p=app(p,end,",healthFadeDurationMs:");p=app_u32(p,end,g_cfg.healthFadeDurationMs);
    p=app(p,end,",healthOpacityPercent:");p=app_u32(p,end,g_cfg.healthOpacityPercent);
    p=app(p,end,",healthShowDuringCombat:");p=app_u32(p,end,g_cfg.healthShowDuringCombat?1:0);
    p=app(p,end,",healthThresholdPercent:");p=app_u32(p,end,g_cfg.healthThresholdPercent);
    p=app(p,end,",missionEnabled:");p=app_u32(p,end,g_cfg.missionEnabled?1:0);
    p=app(p,end,",missionInitialHideDelayMs:");p=app_u32(p,end,g_cfg.missionInitialHideDelayMs);
    p=app(p,end,",missionAfterMapCloseHideDelayMs:");p=app_u32(p,end,g_cfg.missionAfterMapCloseHideDelayMs);
    p=app(p,end,",missionUpdateVisibleMs:");p=app_u32(p,end,g_cfg.missionUpdateVisibleMs);
    p=app(p,end,",missionFadeDurationMs:");p=app_u32(p,end,g_cfg.missionFadeDurationMs);
    p=app(p,end,",missionShowInMap:");p=app_u32(p,end,g_cfg.missionShowInMap?1:0);
    p=app(p,end,",crosshairEnabled:");p=app_u32(p,end,g_cfg.crosshairEnabled?1:0);
    p=app(p,end,",crosshairHideDelayMs:");p=app_u32(p,end,g_cfg.crosshairHideDelayMs);
    p=app(p,end,",crosshairFadeDurationMs:");p=app_u32(p,end,g_cfg.crosshairFadeDurationMs);
    p=app(p,end,",crosshairDotEnabled:");p=app_u32(p,end,g_cfg.crosshairDotEnabled?1:0);
    p=app(p,end,",crosshairDotHideDelayMs:");p=app_u32(p,end,g_cfg.crosshairDotHideDelayMs);
    p=app(p,end,",expeditionEnabled:");p=app_u32(p,end,g_cfg.expeditionEnabled?1:0);
    p=app(p,end,",expeditionHideDelayMs:");p=app_u32(p,end,g_cfg.expeditionHideDelayMs);
    p=app(p,end,",expeditionFadeDurationMs:");p=app_u32(p,end,g_cfg.expeditionFadeDurationMs);

    p=app(p,end,"};");

    char* m=g_menuConfigScript;char* mend=g_menuConfigScript+sizeof(g_menuConfigScript);
    m=app(m,mend,"window.__CDH_MAINMENU_CONFIG={hideNewGame:");
    m=app_u32(m,mend,g_cfg.mainMenuHideNewGame?1:0);
    m=app(m,mend,",hideMissionSelect:");
    m=app_u32(m,mend,g_cfg.mainMenuHideMissionSelect?1:0);
    m=app(m,mend,"};");
}
static bool get_text(void* module,u8*& text,u32& size){
    u8* b=(u8*)module;if(!b||rd16(b)!=0x5A4D)return false;u32 e=rd32(b+0x3C);u8* nt=b+e;if(rd32(nt)!=0x4550)return false;u16 ns=rd16(nt+6),os=rd16(nt+20);u8* sec=nt+24+os;
    for(u16 i=0;i<ns;++i,sec+=40){ if(sec[0]=='.'&&sec[1]=='t'&&sec[2]=='e'&&sec[3]=='x'&&sec[4]=='t'){u32 vs=rd32(sec+8),va=rd32(sec+12);text=b+va;size=vs;return true;} } return false;
}

static const u8 kSlamShowPat[] = {
0x48,0x8B,0x57,0x28,0x4C,0x8D,0x05,0,0,0,0,0x48,0x8B,0xCB,0xE8,0,0,0,0,0xC6,0x87,0xB4,0x01,0x00,0x00,0x01};
static const char kSlamShowMask[] = "xxxxxxx????xxxx????xxxxxxx";

static uptr scan_slam_show_path(void* exe,u32& count){
    count=0;u8* t=nullptr;u32 sz=0;if(!get_text(exe,t,sz))return 0;
    usize plen=sizeof(kSlamShowPat);uptr first=0;
    for(u32 i=0;i+plen<=sz;++i){
        bool ok=true;
        for(usize j=0;j<plen;++j){
            if(kSlamShowMask[j]=='x'&&t[i+j]!=kSlamShowPat[j]){ok=false;break;}
        }
        if(ok){if(!first)first=(uptr)(t+i);++count;i+=(u32)plen-1;}
    }
    return first;
}

static bool patch_ground_slam_target_circle(void* exe){
    if(!g_cfg.hideGroundSlamTargetCircle)return true;
    u32 count=0;uptr hit=scan_slam_show_path(exe,count);
    log_cstr("Ground Slam target show signature matches: ");log_dec(count);
    log_cstr(" first=");log_hex(hit);log_bytes("\r\n",2);
    if(count!=1||!hit){
        log_line("Ground Slam circle patch skipped: show path already modified or unsupported.");
        return false;
    }

    u8* lea=(u8*)hit+4;
    s32 oldDisp=*(s32*)(lea+3);
    const char* show=(const char*)(lea+7+(s64)oldDisp);
    const char* hide=show-0x18;
    if(!streq_ascii(show,"slam_target_show")||!streq_ascii(hide,"slam_target_hide")){
        log_line("Ground Slam circle patch skipped: event strings did not validate.");
        return false;
    }

    s64 nd64=(s64)(uptr)hide-(s64)(uptr)(lea+7);
    if(nd64<(s64)-2147483648LL||nd64>(s64)2147483647LL){
        log_line("Ground Slam circle patch skipped: replacement displacement out of range.");
        return false;
    }
    s32 newDisp=(s32)nd64;
    DWORD old=0;
    if(!g_VirtualProtect((void*)(lea+3),sizeof(s32),PAGE_EXECUTE_READWRITE,&old)){
        log_line("Ground Slam circle patch skipped: VirtualProtect failed.");
        return false;
    }
    *(s32*)(lea+3)=newDisp;
    DWORD dummy=0;
    g_VirtualProtect((void*)(lea+3),sizeof(s32),old,&dummy);
    if(g_FlushInstructionCache)g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)lea,7);
    log_line("Ground Slam target circle disabled: slam_target_show redirected to slam_target_hide.");
    return true;
}

static const u8 kNewGameActionPat[] = {
0x40,0x53,0x48,0x83,0xEC,0x20,0x83,0xB9,0xA0,0x02,0x00,0x00,0x00,0x48,0x8B,0xD9,0x76,0x30};
static const u8 kMissionSelectActionPat[] = {
0x48,0x8B,0xC4,0x55,0x57,0x41,0x56,0x48,0x8D,0x6C,0x24,0x90,0x48,0x81,0xEC,0x70,0x01,0x00,0x00,
0x48,0xC7,0x44,0x24,0x30,0xFE,0xFF,0xFF,0xFF,0x48,0x89,0x58,0x18,0x48,0x89,0x70,0x20};

static uptr scan_exact_text_pattern(void* exe,const u8* pat,usize plen,u32& count){
    count=0;u8* t=nullptr;u32 sz=0;if(!get_text(exe,t,sz))return 0;
    uptr first=0;
    for(u32 i=0;i+plen<=sz;++i){
        bool ok=true;
        for(usize j=0;j<plen;++j){if(t[i+j]!=pat[j]){ok=false;break;}}
        if(ok){if(!first)first=(uptr)(t+i);++count;i+=(u32)plen-1;}
    }
    return first;
}
static bool patch_action_to_ret(uptr target,const char* label){
    if(!target||!g_VirtualProtect)return false;
    DWORD old=0;
    if(!g_VirtualProtect((void*)target,1,PAGE_EXECUTE_READWRITE,&old))return false;
    *(u8*)target=0xC3;
    DWORD dummy=0;g_VirtualProtect((void*)target,1,old,&dummy);
    if(g_FlushInstructionCache)g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)target,1);
    log_cstr(label);log_line(" native action disabled.");
    return true;
}
static void patch_main_menu_actions(void* exe){
    if(g_cfg.mainMenuHideNewGame){
        u32 n=0;uptr p=scan_exact_text_pattern(exe,kNewGameActionPat,sizeof(kNewGameActionPat),n);
        log_cstr("New Game native action signature matches: ");log_dec(n);log_cstr(" first=");log_hex(p);log_bytes("\r\n",2);
        if(n==1)patch_action_to_ret(p,"New Game");
    }
    if(g_cfg.mainMenuHideMissionSelect){
        u32 n=0;uptr p=scan_exact_text_pattern(exe,kMissionSelectActionPat,sizeof(kMissionSelectActionPat),n);
        log_cstr("Mission Select native action signature matches: ");log_dec(n);log_cstr(" first=");log_hex(p);log_bytes("\r\n",2);
        if(n==1)patch_action_to_ret(p,"Mission Select");
    }
}

static const u8 kMenuFactoryPat[] = {
0x48,0x89,0x4C,0x24,0x08,0x48,0x83,0xEC,0x38,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,
0xBA,0x08,0x00,0x00,0x00,0xB9,0xB8,0x07,0x00,0x00,0xFF,0x15,0,0,0,0,
0x48,0x89,0x44,0x24,0x40,0x48,0x8B,0xC8,0xE8,0,0,0,0,0x90,0x48,0x83,0xC4,0x38,0xC3};
static const char kMenuFactoryMask[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxx????xxxxxx";

static uptr scan_menu_factory(void* exe,u32& count){
    count=0;u8* t=nullptr;u32 sz=0;if(!get_text(exe,t,sz))return 0;
    usize plen=sizeof(kMenuFactoryPat);uptr first=0;
    for(u32 i=0;i+plen<=sz;++i){
        bool ok=true;
        for(usize j=0;j<plen;++j){
            if(kMenuFactoryMask[j]=='x'&&t[i+j]!=kMenuFactoryPat[j]){ok=false;break;}
        }
        if(ok){if(!first)first=(uptr)(t+i);++count;i+=(u32)plen-1;}
    }
    return first;
}

static const u8 kMultiLaunchUpdatePat[] = {
0x48,0x8B,0xC4,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18,0x41,0x56,0x48,0x81,0xEC,0xA0,0x00,0x00,0x00};
static const u8 kMultiLaunchHiddenPat[] = {
0x80,0x7E,0x41,0x00,0x74,0x06,0xC6,0x46,0x41,0x00,0xB3,0x01,0x80,0x7E,0x40,0x00};
static const u8 kMultiLaunchHasAimPat[] = {
0x74,0x04,0xB0,0x01,0xEB,0x02,0x32,0xC0,0x48,0x89,0x9C,0x24,0xB0,0x00,0x00,0x00};
static const u8 kCrosshairUpdatePat[] = {
0x48,0x8B,0xC4,0x57,0x48,0x81,0xEC,0xA0,0x03,0x00,0x00,0x48,0xC7,0x44,0x24,0x40,0xFE,0xFF,0xFF,0xFF,
0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x0F,0x29,0x70,0xE8,0x0F,0x29,0x78,0xD8,0x44,0x0F,0x29,0x40,0xC8};
static const u8 kMissionSelectMenuModelPat[] = {
0x44,0x88,0x74,0x24,0x3C,0x44,0x88,0x44,0x24,0x3D,0x66,0xC7,0x44,0x24,0x3E,0x00,0x00};

static uptr scan_exact_one(void* exe,const u8* pat,usize plen,u32& count){
    return scan_exact_text_pattern(exe,pat,plen,count);
}

static u32 scan_masked_all(void* exe,const u8* pat,const char* mask,usize plen,uptr* out,u32 cap){
    u8* t=nullptr;u32 sz=0;if(!get_text(exe,t,sz))return 0;
    u32 count=0;
    for(u32 i=0;i+plen<=sz;++i){
        bool ok=true;
        for(usize j=0;j<plen;++j){
            if(mask[j]=='x'&&t[i+j]!=pat[j]){ok=false;break;}
        }
        if(ok){
            if(count<cap)out[count]=(uptr)(t+i);
            ++count;i+=(u32)plen-1;
        }
    }
    return count;
}

static const u8 kLaunchChangeHighlightPat[] = {
0x48,0x8B,0x4F,0x08,0x48,0x8D,0x15,0,0,0,0,0x48,0x8B,0x49,0x28,0xFF,0x15,0,0,0,0};
static const char kLaunchChangeHighlightMask[] = "xxxxxxx????xxxxxx????";
static const u8 kLaunchStartHighlightPat[] = {
0x48,0x8B,0x4B,0x08,0x48,0x8D,0x15,0,0,0,0,0x48,0x8B,0x49,0x28,
0x48,0x83,0xC4,0x20,0x5B,0x48,0xFF,0x25,0,0,0,0};
static const char kLaunchStartHighlightMask[] = "xxxxxxx????xxxxxxxxxxxx????";

static bool cstr_eq_exact(const char* a,const char* b){
    if(!a||!b)return false;
    usize i=0;
    for(;;++i){
        char ca=a[i],cb=b[i];
        if(ca!=cb)return false;
        if(!ca)return true;
        if(i>160)return false;
    }
}

static bool locate_launch_selection_highlights(void* exe){
    static const char kChangeEvent[]="global_events\\launch_change_selection_highlight";
    static const char kStartEvent[]="global_events\\launch_start_selection_highlight";

    uptr rawChange[8]={0,0,0,0,0,0,0,0};
    u32 rawNc=scan_masked_all(exe,kLaunchChangeHighlightPat,kLaunchChangeHighlightMask,
                              sizeof(kLaunchChangeHighlightPat),rawChange,8);
    uptr change[2]={0,0};u32 nc=0;
    for(u32 i=0;i<rawNc&&i<8;++i){
        uptr p=rawChange[i];
        s32 rel=*(s32*)(p+7);
        const char* eventName=(const char*)(p+11+(s64)rel);
        if(cstr_eq_exact(eventName,kChangeEvent)&&nc<2)change[nc++]=p;
    }

    uptr rawStart[4]={0,0,0,0};
    u32 rawNs=scan_masked_all(exe,kLaunchStartHighlightPat,kLaunchStartHighlightMask,
                              sizeof(kLaunchStartHighlightPat),rawStart,4);
    uptr start=0;u32 ns=0;
    for(u32 i=0;i<rawNs&&i<4;++i){
        uptr p=rawStart[i];
        s32 rel=*(s32*)(p+7);
        const char* eventName=(const char*)(p+11+(s64)rel);
        if(cstr_eq_exact(eventName,kStartEvent)){start=p;++ns;}
    }

    log_cstr("Launch change dispatch pattern raw=");log_dec(rawNc);
    log_cstr(" exact-event=");log_dec(nc);
    if(nc){log_cstr(" first=");log_hex(change[0]);}
    log_bytes("\r\n",2);
    log_cstr("Launch start/stop dispatch pattern raw=");log_dec(rawNs);
    log_cstr(" exact-start=");log_dec(ns);
    if(start){log_cstr(" start=");log_hex(start);}
    log_bytes("\r\n",2);

    if(nc!=2||ns!=1||!start)return false;
    for(int i=0;i<2;++i){
        g_launchChangeDispatch[i]=(u8*)(change[i]+15);
        for(int j=0;j<6;++j)g_launchChangeOriginal[i][j]=g_launchChangeDispatch[i][j];
    }
    g_launchStartTail=(u8*)(start+20);
    for(int j=0;j<7;++j)g_launchStartOriginal[j]=g_launchStartTail[j];
    return true;
}

static void set_launch_selection_highlight_suppressed(bool suppress){
    if(!g_launchChangeDispatch[0]||!g_launchChangeDispatch[1]||!g_launchStartTail||
       suppress==g_launchSelectionSuppressed||!g_VirtualProtect)return;

    for(int i=0;i<2;++i){
        DWORD old=0;
        if(!g_VirtualProtect(g_launchChangeDispatch[i],6,PAGE_EXECUTE_READWRITE,&old))return;
        if(suppress){
            for(int j=0;j<6;++j)g_launchChangeDispatch[i][j]=0x90;
        }else{
            for(int j=0;j<6;++j)g_launchChangeDispatch[i][j]=g_launchChangeOriginal[i][j];
        }
        DWORD dummy=0;g_VirtualProtect(g_launchChangeDispatch[i],6,old,&dummy);
        if(g_FlushInstructionCache)
            g_FlushInstructionCache((HANDLE)(uptr)-1,g_launchChangeDispatch[i],6);
    }

    DWORD old=0;
    if(!g_VirtualProtect(g_launchStartTail,7,PAGE_EXECUTE_READWRITE,&old))return;
    if(suppress){
        g_launchStartTail[0]=0xC3;
        for(int j=1;j<7;++j)g_launchStartTail[j]=0x90;
    }else{
        for(int j=0;j<7;++j)g_launchStartTail[j]=g_launchStartOriginal[j];
    }
    DWORD dummy=0;g_VirtualProtect(g_launchStartTail,7,old,&dummy);
    if(g_FlushInstructionCache)
        g_FlushInstructionCache((HANDLE)(uptr)-1,g_launchStartTail,7);

    g_launchSelectionSuppressed=suppress;
    log_line(suppress?
      "Launch start/change selection-highlight events suppressed; stop event left intact.":
      "Launch start/change selection-highlight events restored.");
}

static const u8 kTargetIndicatorSetterPat[] = {
0x48,0x8B,0x15,0,0,0,0,0x85,0xC9,0x0F,0x95,0xC0,0x88,0x82,0x51,0x02,0x00,0x00,
0xC6,0x82,0xE8,0x04,0x00,0x00,0x01,0xC3};
static const char kTargetIndicatorSetterMask[] = "xxx????xxxxxxxxxxxxxxxxxxx";

static bool locate_interface_options(void* exe){
    u8* t=nullptr;u32 sz=0;if(!get_text(exe,t,sz))return false;
    const usize plen=sizeof(kTargetIndicatorSetterPat);
    uptr first=0;u32 count=0;
    for(u32 i=0;i+plen<=sz;++i){
        bool ok=true;
        for(usize j=0;j<plen;++j){
            if(kTargetIndicatorSetterMask[j]=='x'&&t[i+j]!=kTargetIndicatorSetterPat[j]){ok=false;break;}
        }
        if(ok){if(!first)first=(uptr)(t+i);++count;i+=(u32)plen-1;}
    }
    log_cstr("InterfaceOptions Target Indicator setter matches: ");log_dec(count);
    log_cstr(" first=");log_hex(first);log_bytes("\r\n",2);
    if(count!=1||!first)return false;
    s32 rel=*(s32*)(first+3);
    g_interfaceOptionsPtrSlot=(uptr*)(first+7+(s64)rel);
    log_cstr("InterfaceOptions global pointer slot: ");log_hex((uptr)g_interfaceOptionsPtrSlot);log_bytes("\r\n",2);
    return true;
}

static void apply_native_target_indicator(bool forced){
    if(!g_interfaceOptionsPtrSlot)return;
    uptr opts=rdptr(g_interfaceOptionsPtrSlot);
    if(!opts)return;
    if(!g_interfaceOptionsSaved){
        g_savedTargetIndicator=rd8((void*)(opts+0x251));
        g_interfaceOptionsSaved=true;
        log_cstr("Native Target Indicator original value=");log_dec(g_savedTargetIndicator?1:0);log_bytes("\r\n",2);
    }
    bool wantHide=g_cfg.multiLaunchHideObjectIndicators&&g_multiLaunchPatchEnabled&&!forced;
    u8 desired=wantHide?0:g_savedTargetIndicator;
    if(rd8((void*)(opts+0x251))!=desired){
        *(volatile u8*)(opts+0x251)=desired;
        *(volatile u8*)(opts+0x4E8)=1;
    }
    if(wantHide!=g_targetIndicatorOverrideActive){
        g_targetIndicatorOverrideActive=wantHide;
        log_line(wantHide?
          "Native InterfaceOptions Target Indicator forced OFF while Multi Launch suppression is active.":
          "Native InterfaceOptions Target Indicator restored.");
    }
}

static bool patch_mission_select_menu_model(void* exe){
    if(!g_cfg.mainMenuHideMissionSelect)return true;
    u32 n=0;uptr p=scan_exact_one(exe,kMissionSelectMenuModelPat,sizeof(kMissionSelectMenuModelPat),n);
    log_cstr("Mission Select MenuOptions signature matches: ");log_dec(n);
    log_cstr(" first=");log_hex(p);log_bytes("\r\n",2);
    if(n!=1||!p)return false;
    // Replace "mov [rsp+3Dh], r8b" with "mov byte ptr [rsp+3Dh],0".
    const u8 repl[5]={0xC6,0x44,0x24,0x3D,0x00};
    DWORD old=0;
    if(!g_VirtualProtect((void*)(p+5),5,PAGE_EXECUTE_READWRITE,&old))return false;
    for(int i=0;i<5;++i)((u8*)p)[5+i]=repl[i];
    DWORD dummy=0;g_VirtualProtect((void*)(p+5),5,old,&dummy);
    if(g_FlushInstructionCache)g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)(p+5),5);
    g_missionSelectModelPatched=true;
    log_line("Mission Select removed from native MenuOptions model.");
    return true;
}

static bool locate_multilaunch_suppression_instructions(void* exe){
    u32 nh=0;uptr hp=scan_exact_one(exe,kMultiLaunchHiddenPat,sizeof(kMultiLaunchHiddenPat),nh);
    log_cstr("Multi Launch hidden-state instruction matches: ");log_dec(nh);
    log_cstr(" first=");log_hex(hp);log_bytes("\r\n",2);

    u32 na=0;uptr ap=scan_exact_one(exe,kMultiLaunchHasAimPat,sizeof(kMultiLaunchHasAimPat),na);
    log_cstr("Multi Launch HasAimTarget instruction matches: ");log_dec(na);
    log_cstr(" first=");log_hex(ap);log_bytes("\r\n",2);

    if(nh!=1||!hp||na!=1||!ap)return false;
    g_multiHiddenCmpImm=(u8*)(hp+3);
    g_multiHiddenMovImm=(u8*)(hp+9);
    g_multiHasAimForceBytes=(u8*)(ap+2);
    return true;
}

static void set_multilaunch_suppression_code(bool suppress){
    if(!g_multiHiddenCmpImm||!g_multiHiddenMovImm||!g_multiHasAimForceBytes||
       suppress==g_multiLaunchPatchEnabled)return;

    DWORD oldAim=0;
    if(!g_VirtualProtect(g_multiHasAimForceBytes,2,PAGE_EXECUTE_READWRITE,&oldAim))return;
    if(suppress){
        // Vanilla B0 01 = mov al,1. 32 C0 = xor al,al.
        // This makes the updater itself publish HasAimTarget=false.
        g_multiHasAimForceBytes[0]=0x32;
        g_multiHasAimForceBytes[1]=0xC0;
    }else{
        g_multiHasAimForceBytes[0]=0xB0;
        g_multiHasAimForceBytes[1]=0x01;
    }
    DWORD dummyAim=0;g_VirtualProtect(g_multiHasAimForceBytes,2,oldAim,&dummyAim);

    DWORD oldHidden=0;
    if(!g_VirtualProtect(g_multiHiddenCmpImm,16,PAGE_EXECUTE_READWRITE,&oldHidden))return;
    *g_multiHiddenCmpImm=suppress?1:0;
    *g_multiHiddenMovImm=suppress?1:0;
    DWORD dummyHidden=0;g_VirtualProtect(g_multiHiddenCmpImm,16,oldHidden,&dummyHidden);

    if(g_FlushInstructionCache){
        g_FlushInstructionCache((HANDLE)(uptr)-1,g_multiHasAimForceBytes,2);
        g_FlushInstructionCache((HANDLE)(uptr)-1,g_multiHiddenCmpImm,16);
    }
    g_multiLaunchPatchEnabled=suppress;
    log_line(suppress?
      "Multi Launch updater now publishes HasAimTarget=false + ReticuleHidden=true before Coherent notification.":
      "Multi Launch updater restored vanilla HasAimTarget/ReticuleHidden publication.");
}

static int multi_slot_from_index(u32 idx){return idx==1?1:(idx==2?2:0);}
static uptr multi_model_from_slot(void* block,int slot){
    static const uptr offs[3]={0x88,0x120,0x1B8};
    return block?(uptr)block+offs[slot]:0;
}

static void WINAPI HookMultiLaunchUpdate(void* block,u32 idx,void* a3,void* targetInfo){
    int slot=multi_slot_from_index(idx);
    u64 now=g_GetTickCount64?g_GetTickCount64():0;
    bool forced=g_forceHudUntilMs&&now<g_forceHudUntilMs;
    bool suppress=false;

    if(g_cfg.multiLaunchHideObjectIndicators&&!forced){
        if(g_cfg.multiLaunchHideDelayMs==0){
            suppress=true;
        }else{
            bool active=false;
            if(targetInfo){
                uptr raw=rdptr(targetInfo);
                uptr handle=(raw>>14)&0x0000FFFFFFFFFFFFull;
                active=handle!=0;
            }
            if(block){
                uptr model=multi_model_from_slot(block,slot);
                if(rd8((void*)(model+0x40))!=0)active=true;
            }
            g_multiLaunchSlotActive[slot]=active;
            bool any=g_multiLaunchSlotActive[0]||g_multiLaunchSlotActive[1]||g_multiLaunchSlotActive[2];
            if(any){
                if(!g_multiLaunchActiveSinceMs)g_multiLaunchActiveSinceMs=now;
                u64 elapsed=now>=g_multiLaunchActiveSinceMs?now-g_multiLaunchActiveSinceMs:0;
                suppress=elapsed>=g_cfg.multiLaunchHideDelayMs;
            }else{
                g_multiLaunchActiveSinceMs=0;
            }
        }
    }else{
        g_multiLaunchActiveSinceMs=0;
    }

    set_launch_selection_highlight_suppressed(suppress);

    if(g_originalMultiLaunchUpdate)g_originalMultiLaunchUpdate(block,idx,a3,targetInfo);
}

static void WINAPI HookCrosshairUpdate(void* crosshair,void* a2,void* source){
    u64 now=g_GetTickCount64?g_GetTickCount64():0;
    bool forced=g_forceHudUntilMs&&now<g_forceHudUntilMs;
    bool hide=false;
    if(g_cfg.crosshairDotEnabled&&!forced){
        if(!g_crosshairDotSinceMsNative)g_crosshairDotSinceMsNative=now;
        u64 elapsed=now>=g_crosshairDotSinceMsNative?now-g_crosshairDotSinceMsNative:0;
        hide=elapsed>=g_cfg.crosshairDotHideDelayMs;
    }else{
        g_crosshairDotSinceMsNative=0;
    }

    if(!g_originalCrosshairUpdate){return;}
    if(hide&&source){
        float* minSize=(float*)((u8*)source+0xAC);
        float old=*minSize;
        *minSize=0.0f;
        g_originalCrosshairUpdate(crosshair,a2,source);
        *minSize=old;
        if(!g_crosshairDotNativeLogOnce){
            log_line("Crosshair minimum-reticle size forced to 0 before CrosshairData notification.");
            g_crosshairDotNativeLogOnce=true;
        }
        return;
    }
    g_originalCrosshairUpdate(crosshair,a2,source);
}

static void write_abs_jump(u8* at,void* dst);

static bool install_code_detour(WinApi& api,uptr target,void* hook,void** original,usize stolen){
    u8* tramp=(u8*)api.VirtualAlloc(nullptr,96,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!tramp)return false;
    for(usize i=0;i<stolen;++i)tramp[i]=((u8*)target)[i];
    write_abs_jump(tramp+stolen,(void*)(target+stolen));
    *original=tramp;
    DWORD old=0;
    if(!api.VirtualProtect((void*)target,stolen,PAGE_EXECUTE_READWRITE,&old))return false;
    write_abs_jump((u8*)target,hook);
    for(usize i=14;i<stolen;++i)((u8*)target)[i]=0x90;
    DWORD dummy=0;api.VirtualProtect((void*)target,stolen,old,&dummy);
    api.FlushInstructionCache((HANDLE)(uptr)-1,(void*)target,stolen);
    return true;
}

static bool install_multilaunch_update_hook(WinApi& api,void* exe){
    if(!g_cfg.multiLaunchHideObjectIndicators)return true;
    u32 n=0;uptr p=scan_exact_one(exe,kMultiLaunchUpdatePat,sizeof(kMultiLaunchUpdatePat),n);
    log_cstr("Multi Launch updater signature matches: ");log_dec(n);
    log_cstr(" first=");log_hex(p);log_bytes("\r\n",2);
    if(n!=1||!p||!locate_launch_selection_highlights(exe))return false;
    if(!install_code_detour(api,p,(void*)&HookMultiLaunchUpdate,(void**)&g_originalMultiLaunchUpdate,20))return false;
    if(g_cfg.multiLaunchHideDelayMs==0)set_launch_selection_highlight_suppressed(true);
    log_line("Multi Launch updater hook installed; exact Launch selection-highlight dispatch suppression armed.");
    return true;
}

static bool install_crosshair_dot_hook(WinApi& api,void* exe){
    if(!g_cfg.crosshairDotEnabled)return true;
    u32 n=0;uptr p=scan_exact_one(exe,kCrosshairUpdatePat,sizeof(kCrosshairUpdatePat),n);
    log_cstr("CrosshairData updater signature matches: ");log_dec(n);
    log_cstr(" first=");log_hex(p);log_bytes("\r\n",2);
    if(n!=1||!p)return false;
    if(!install_code_detour(api,p,(void*)&HookCrosshairUpdate,(void**)&g_originalCrosshairUpdate,20))return false;
    log_line("Native CrosshairDot updater hook installed.");
    return true;
}

static const u8 kFactoryPat[] = {
0x48,0x89,0x4C,0x24,0x08,0x53,0x48,0x83,0xEC,0x30,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,
0x49,0x8B,0xD8,0xBA,0x08,0x00,0x00,0x00,0xB9,0x10,0x03,0x00,0x00,0xFF,0x15,0,0,0,0,
0x48,0x89,0x44,0x24,0x40,0x48,0x8B,0x13,0x48,0x8B,0xC8,0xE8,0,0,0,0,0x90,0x48,0x83,0xC4,0x30,0x5B,0xC3};
static const char kFactoryMask[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx????xxxxxxx";

static uptr scan_factory(void* exe,u32& count){
    count=0;u8* t=nullptr;u32 sz=0;if(!get_text(exe,t,sz))return 0;usize plen=sizeof(kFactoryPat);uptr first=0;
    for(u32 i=0;i+plen<=sz;++i){bool ok=true;for(usize j=0;j<plen;++j){if(kFactoryMask[j]=='x'&&t[i+j]!=kFactoryPat[j]){ok=false;break;}}if(ok){if(!first)first=(uptr)(t+i);++count;i+=(u32)plen-1;}}
    return first;
}

static void write_abs_jump(u8* at,void* dst){ at[0]=0xFF;at[1]=0x25;at[2]=0;at[3]=0;at[4]=0;at[5]=0;*(uptr*)(at+6)=(uptr)dst; }

static void restore_ready_hook(){
    if(!g_readyEntry||!g_originalReady||!g_VirtualProtect)return;
    DWORD old=0;
    if(g_VirtualProtect((void*)g_readyEntry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        *g_readyEntry=(uptr)g_originalReady;
        DWORD dummy=0;
        g_VirtualProtect((void*)g_readyEntry,sizeof(uptr),old,&dummy);
        if(g_FlushInstructionCache) g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)g_readyEntry,sizeof(uptr));
        log_line("Ready vtable hook restored.");
    }
    g_readyEntry=nullptr;
}

static void restore_update_hook(){
    if(!g_updateEntry||!g_originalUpdate||!g_VirtualProtect)return;
    DWORD old=0;
    if(g_VirtualProtect((void*)g_updateEntry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        *g_updateEntry=(uptr)g_originalUpdate;
        DWORD dummy=0;
        g_VirtualProtect((void*)g_updateEntry,sizeof(uptr),old,&dummy);
        if(g_FlushInstructionCache) g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)g_updateEntry,sizeof(uptr));
        log_line("HUD update hotkey hook restored.");
    }
    g_updateEntry=nullptr;
}

static void native_hide_launch_reticles(void* hud,u64 now){
    if(!hud)return;
    uptr launch=rdptr((u8*)hud+0x168);
    if(!launch)return;

    bool forced=g_forceHudUntilMs&&now<g_forceHudUntilMs;
    if(forced){
        g_launchReticleSinceMs=0;
        return;
    }

    // LaunchIndicator is the first object in the Launch block.
    // m_bIsReticuleHidden is offset +0x19.
    if(g_cfg.launchHideReticle){
        u8* hidden=(u8*)(launch+0x19);
        if(*hidden==0){
            if(!g_launchReticleSinceMs)g_launchReticleSinceMs=now;
            u64 elapsed=now>=g_launchReticleSinceMs?now-g_launchReticleSinceMs:0;
            if(elapsed>=g_cfg.launchHideDelayMs){
                *hidden=1;
                if(!g_launchReticleLogOnce){
                    log_line("Launch target reticle forced hidden through LaunchIndicator.m_bIsReticuleHidden.");
                    g_launchReticleLogOnce=true;
                }
            }
        }else{
            g_launchReticleSinceMs=0;
        }
    }else{
        g_launchReticleSinceMs=0;
    }

}


static void WINAPI HookUpdate(void* self){
    if(g_originalUpdate)g_originalUpdate(self);

    u64 now=g_GetTickCount64?g_GetTickCount64():0;

    if(g_GetAsyncKeyState&&g_showHudVk){
        bool down=(g_GetAsyncKeyState((int)g_showHudVk)&0x8000)!=0;
        if(!down){
            g_showHudKeyDown=false;
        }else if(!g_showHudKeyDown){
            g_showHudKeyDown=true;
            if(g_GetTickCount64)g_forceHudUntilMs=now+(u64)g_cfg.showHudDurationMs;
            g_crosshairDotSinceMsNative=0;
            g_multiLaunchActiveSinceMs=0;
            g_multiLaunchDiagLogged[0]=g_multiLaunchDiagLogged[1]=g_multiLaunchDiagLogged[2]=false;

            void* view=(self&&g_pageGetView)?g_pageGetView(self):nullptr;
            uptr viewVtable=view?rdptr(view):0;
            uptr execute=viewVtable?rdptr((u8*)viewVtable+61u*sizeof(uptr)):0;
            uptr expectedVtable=(uptr)g_coherentBase+kPublicViewVtableRva;
            uptr expected=(uptr)g_coherentBase+kExecuteScriptRva;
            if(view&&viewVtable==expectedVtable&&execute==expected){
                log_cstr("Show HUD hotkey pressed: ");log_cstr(g_cfg.showHudKey);log_bytes("\r\n",2);
                ((ExecuteScriptFn)execute)(view,kForceHudScript,nullptr);
            }else{
                log_line("Show HUD hotkey pressed but current HUD View is not ready; ignored.");
            }
        }
    }

    bool forcedNow=g_forceHudUntilMs&&now<g_forceHudUntilMs;
    if(g_cfg.multiLaunchHideObjectIndicators&&g_cfg.multiLaunchHideDelayMs==0)
        set_launch_selection_highlight_suppressed(!forcedNow);
    else if(forcedNow)
        set_launch_selection_highlight_suppressed(false);
    native_hide_launch_reticles(self,now);
}

static void restore_menu_ready_hook(){
    if(!g_menuReadyEntry||!g_originalMenuReady||!g_VirtualProtect)return;
    DWORD old=0;
    if(g_VirtualProtect((void*)g_menuReadyEntry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        *g_menuReadyEntry=(uptr)g_originalMenuReady;
        DWORD dummy=0;g_VirtualProtect((void*)g_menuReadyEntry,sizeof(uptr),old,&dummy);
        if(g_FlushInstructionCache)g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)g_menuReadyEntry,sizeof(uptr));
    }
    g_menuReadyEntry=nullptr;
}
static void WINAPI HookMenuReady(void* self){
    u32 n=++g_menuReadyCalls;
    if(g_originalMenuReady)g_originalMenuReady(self);
    log_cstr("Main menu onReadyForBindings #");log_dec(n);log_bytes("\r\n",2);
    void* view=(self&&g_pageGetView)?g_pageGetView(self):nullptr;
    uptr vt=view?rdptr(view):0;
    uptr exec=vt?rdptr((u8*)vt+61u*sizeof(uptr)):0;
    uptr expectedVt=(uptr)g_coherentBase+kPublicViewVtableRva;
    uptr expected=(uptr)g_coherentBase+kExecuteScriptRva;
    if(!view||vt!=expectedVt||exec!=expected)return;
    ((ExecuteScriptFn)exec)(view,g_menuConfigScript,nullptr);
    ((ExecuteScriptFn)exec)(view,kMainMenuScript,nullptr);
    log_line("Main menu cleanup script injected.");
}
static bool install_menu_ready_hook(void* menu){
    if(!menu||!g_VirtualProtect)return false;
    uptr vt=rdptr(menu);if(!vt)return false;
    uptr* entry=(uptr*)(vt+28u*sizeof(uptr));
    uptr original=*entry;if(!original)return false;
    g_originalMenuReady=(ReadyFn)original;g_menuReadyEntry=entry;
    DWORD old=0;
    if(!g_VirtualProtect((void*)entry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        g_menuReadyEntry=nullptr;g_originalMenuReady=nullptr;return false;
    }
    *entry=(uptr)&HookMenuReady;
    DWORD dummy=0;g_VirtualProtect((void*)entry,sizeof(uptr),old,&dummy);
    if(g_FlushInstructionCache)g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)entry,sizeof(uptr));
    log_line("Main menu onReadyForBindings hook installed.");
    return true;
}
static void* WINAPI HookMenuFactory(void* a){
    void* r=g_originalMenuFactory?g_originalMenuFactory(a):nullptr;
    if(r&&!g_menuReadyEntry)install_menu_ready_hook(r);
    return r;
}
static bool install_menu_factory_hook(WinApi& api,uptr target){
    constexpr usize stolen=18;
    u8* tramp=(u8*)api.VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!tramp)return false;
    for(usize i=0;i<stolen;++i)tramp[i]=((u8*)target)[i];
    write_abs_jump(tramp+stolen,(void*)(target+stolen));
    g_originalMenuFactory=(MenuFactoryFn)tramp;
    DWORD old=0;if(!api.VirtualProtect((void*)target,stolen,PAGE_EXECUTE_READWRITE,&old))return false;
    write_abs_jump((u8*)target,(void*)&HookMenuFactory);
    for(usize i=14;i<stolen;++i)((u8*)target)[i]=0x90;
    DWORD dummy=0;api.VirtualProtect((void*)target,stolen,old,&dummy);
    api.FlushInstructionCache((HANDLE)(uptr)-1,(void*)target,stolen);
    return true;
}

static void WINAPI HookReady(void* self){
    u32 n=++g_readyCalls;
    if(g_originalReady) g_originalReady(self);

    void* view=(self&&g_pageGetView)?g_pageGetView(self):nullptr;
    uptr viewPage=view?rdptr((u8*)view+kViewPageOffset):0;
    uptr viewVtable=view?rdptr(view):0;
    uptr execute=viewVtable?rdptr((u8*)viewVtable+61u*sizeof(uptr)):0;
    uptr expectedVtable=(uptr)g_coherentBase+kPublicViewVtableRva;
    uptr expected=(uptr)g_coherentBase+kExecuteScriptRva;

    log_cstr("HUD onReadyForBindings #");log_dec(n);
    log_cstr(" View*=");log_hex((uptr)view);
    log_cstr(" ViewVtable=");log_hex(viewVtable);
    log_cstr(" ViewPage*=");log_hex(viewPage);
    log_cstr(" ExecuteScript=");log_hex(execute);
    log_bytes("\r\n",2);

    if(!view||!viewPage||!viewVtable||!execute){
        log_line("FAIL-OPEN: ready callback did not expose an injectable View; restoring lifecycle hooks.");
        restore_ready_hook();restore_update_hook();
        return;
    }
    if(viewVtable!=expectedVtable){
        log_cstr("FAIL-OPEN: public View vtable mismatch; expected ");log_hex(expectedVtable);
        log_cstr(" got ");log_hex(viewVtable);log_bytes("\r\n",2);
        restore_ready_hook();restore_update_hook();
        return;
    }
    if(execute!=expected){
        log_cstr("FAIL-OPEN: public View vtable[61] mismatch; expected ");log_hex(expected);
        log_cstr(" got ");log_hex(execute);log_bytes("\r\n",2);
        restore_ready_hook();restore_update_hook();
        return;
    }

    log_line("Calling View::ExecuteScript through vtable[61] for this HUD binding context.");
    ((ExecuteScriptFn)execute)(view,g_configScript,nullptr);
    ((ExecuteScriptFn)execute)(view,kSuiteScript,nullptr);
    log_line("Config + cumulative HUD suite injected for current binding context; slot-28 hook remains active.");
}

static bool install_update_hook(void* hud){
    if(!hud||!g_VirtualProtect||!g_GetAsyncKeyState||!g_showHudVk)return false;
    uptr vtbl=rdptr(hud);
    if(!vtbl)return false;
    uptr* entry=(uptr*)(vtbl+27u*sizeof(uptr));
    uptr original=*entry;
    if(!original)return false;

    g_originalUpdate=(UpdateFn)original;
    g_updateEntry=entry;
    log_cstr("HUD update vtable entry[27] hotkey hook: ");log_hex((uptr)entry);
    log_cstr(" original=");log_hex(original);log_bytes("\r\n",2);

    DWORD old=0;
    if(!g_VirtualProtect((void*)entry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        g_updateEntry=nullptr;g_originalUpdate=nullptr;return false;
    }
    *entry=(uptr)&HookUpdate;
    DWORD dummy=0;
    g_VirtualProtect((void*)entry,sizeof(uptr),old,&dummy);
    if(g_FlushInstructionCache)g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)entry,sizeof(uptr));
    log_line("Native Show HUD hotkey hook installed.");
    return true;
}

static bool install_ready_hook(void* hud){
    if(!hud||!g_VirtualProtect)return false;
    uptr vtbl=rdptr(hud);
    if(!vtbl)return false;
    uptr* entry=(uptr*)(vtbl + 28u*sizeof(uptr));
    uptr original=*entry;
    if(!original)return false;

    g_originalReady=(ReadyFn)original;
    g_readyEntry=entry;

    log_cstr("HUD onReadyForBindings vtable entry[28]: ");log_hex((uptr)entry);
    log_cstr(" original=");log_hex(original);log_bytes("\r\n",2);

    DWORD old=0;
    if(!g_VirtualProtect((void*)entry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        g_readyEntry=nullptr; g_originalReady=nullptr; return false;
    }
    *entry=(uptr)&HookReady;
    DWORD dummy=0;
    g_VirtualProtect((void*)entry,sizeof(uptr),old,&dummy);
    if(g_FlushInstructionCache) g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)entry,sizeof(uptr));
    log_line("Persistent onReadyForBindings lifecycle hook installed.");
    return true;
}

static void* WINAPI HookFactory(void* a,void* b,void* c,void* d){
    u32 n=++g_factoryCalls;
    log_cstr("HUD factory call #");log_dec(n);log_cstr(" arg3=");log_hex((uptr)c);log_bytes("\r\n",2);
    void* r=g_originalFactory?g_originalFactory(a,b,c,d):nullptr;
    log_cstr("HUD object returned: ");log_hex((uptr)r);log_bytes("\r\n",2);
    if(r){
        uptr vtbl=rdptr((u8*)r+0x00);
        void* system=(void*)rdptr((u8*)r+0x10);
        u32 pageIndex=rd32((u8*)r+0x18);
        log_cstr("HUD vtable: ");log_hex(vtbl);log_bytes("\r\n",2);
        log_cstr("Initial HUD System*: ");log_hex((uptr)system);log_cstr(" pageIndex=");log_dec(pageIndex);log_bytes("\r\n",2);
        if(!g_readyEntry && !install_ready_hook(r)) log_line("FAIL-OPEN: could not install persistent ready hook.");
        if(!g_updateEntry){
            if(g_GetAsyncKeyState&&g_showHudVk){
                if(!install_update_hook(r))log_line("Show HUD hotkey hook could not be installed.");
            }else{
                log_line("Show HUD native hotkey unavailable or key name unsupported.");
            }
        }
    }
    return r;
}

static bool install_hook(WinApi& api,uptr target){
    constexpr usize stolen=19;
    u8* tramp=(u8*)api.VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tramp)return false;
    for(usize i=0;i<stolen;++i)tramp[i]=((u8*)target)[i];
    write_abs_jump(tramp+stolen,(void*)(target+stolen));
    g_originalFactory=(FactoryFn)tramp;
    DWORD old=0;if(!api.VirtualProtect((void*)target,stolen,PAGE_EXECUTE_READWRITE,&old))return false;
    write_abs_jump((u8*)target,(void*)&HookFactory);for(usize i=14;i<stolen;++i)((u8*)target)[i]=0x90;
    DWORD dummy=0;api.VirtualProtect((void*)target,stolen,old,&dummy);api.FlushInstructionCache((HANDLE)(uptr)-1,(void*)target,stolen);return true;
}

extern "C" __declspec(dllexport) int WINAPI CDH_Version(){return 118;}
extern "C" __declspec(dllexport) void* CDH_RelocAnchor=(void*)&CDH_Version;

extern "C" BOOL WINAPI DllMain(HMODULE,DWORD reason,LPVOID){
    if(reason!=DLL_PROCESS_ATTACH)return 1; WinApi api{};if(!init_api(api))return 1;
    g_VirtualProtect=api.VirtualProtect;
    g_FlushInstructionCache=api.FlushInstructionCache;
    g_GetAsyncKeyState=api.GetAsyncKeyState;
    g_GetTickCount64=api.GetTickCount64;
    load_config(api);g_showHudVk=parse_vk(g_cfg.showHudKey);build_config_script();
    g_WriteFile=api.WriteFile;g_log=api.CreateFileW(L"plugins\\ControlDynamicHUD.log",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!g_log||(uptr)g_log==INVALID_HANDLE_VALUE_U)return 1;
    log_line("Control Dynamic HUD V1.0S EXACT EVENT + DOT MASK TEST");log_line("Mode: exact Launch selection events + center-pixel mask + New Game branch pruning");
    log_cstr("Config: Enabled=");log_dec(g_cfg.enabled?1:0);log_cstr(" ShowHUDKey=");log_cstr(g_cfg.showHudKey);log_cstr(" VK=");log_hex(g_showHudVk);log_cstr(" ShowHUDDurationMs=");log_dec(g_cfg.showHudDurationMs);
    log_cstr(" HideNewGame=");log_dec(g_cfg.mainMenuHideNewGame?1:0);
    log_cstr(" HideMissionSelect=");log_dec(g_cfg.mainMenuHideMissionSelect?1:0);
    log_bytes("\r\n",2);
    if(!g_cfg.enabled){log_line("Mod disabled by INI; no hook installed.");return 1;}
    log_cstr("Executable: ");log_wide(first_module_name());log_bytes("\r\n",2);void* exe=first_module_base();log_cstr("Executable base: ");log_hex((uptr)exe);log_bytes("\r\n",2);
    patch_ground_slam_target_circle(exe);
    patch_main_menu_actions(exe);
    log_line("V1.0Q Target Indicator path rejected by in-game test; native option override disabled.");
    if(g_cfg.mainMenuHideMissionSelect&&!patch_mission_select_menu_model(exe))
        log_line("Mission Select MenuOptions suppression not installed; DOM fallback remains active.");
    if(g_cfg.mainMenuHideNewGame)
        log_line("New Game has no MenuOptions visibility flag; V1.0S will prune its complete Coherent branch.");
    if(!install_multilaunch_update_hook(api,exe))
        log_line("Multi Launch pre-notification hook not installed; feature fails open.");
    log_line("CrosshairDot V1.0S adds a resolution-scaled transparent center mask to the entire crosshair component, independent of DOM child structure.");
    void* coh=find_module("coherentuigt.dll");g_coherentBase=coh;log_cstr("CoherentUIGT.dll: ");if(coh){log_hex((uptr)coh);log_cstr(" (loaded)");}else log_cstr("NOT FOUND");log_bytes("\r\n",2);
    void* ui=find_module("ui_rmdwin10_f.dll");log_cstr("ui_rmdwin10_f.dll: ");if(ui){log_hex((uptr)ui);log_cstr(" (loaded)");}else log_cstr("NOT FOUND");log_bytes("\r\n",2);
    const char* pageGetViewName="?getView@Page@ui@@QEAAPEAVView@UIGT@Coherent@@XZ";
    g_pageGetView=ui?(PageGetViewFn)resolve_export(ui,pageGetViewName):nullptr;
    log_cstr("ui::Page::getView export: ");if(g_pageGetView)log_hex((uptr)g_pageGetView);else log_cstr("NOT FOUND");log_bytes("\r\n",2);
    if(!coherent_build_matches(coh)){
        log_line("FAIL-OPEN: unsupported CoherentUIGT build; no hook installed.");
        return 1;
    }
    u32 count=0;uptr factory=scan_factory(exe,count);log_cstr("HUD factory signature matches: ");log_dec(count);log_cstr(" first=");log_hex(factory);log_bytes("\r\n",2);
    if(count!=1||!factory){log_line("FAIL-OPEN: unique HUD factory not found; no hook installed.");return 1;}
    if(!g_pageGetView){log_line("FAIL-OPEN: ui::Page::getView export not found; no hook installed.");return 1;}
    s32 rel=*(s32*)(factory+0x32);uptr ctor=factory+0x36+(s64)rel;
    log_cstr("HUD constructor target: ");log_hex(ctor);log_bytes("\r\n",2);
    if(!install_hook(api,factory)){log_line("FAIL-OPEN: hook installation failed.");return 1;}
    log_line("HUD factory hook installed.");

    if(g_cfg.mainMenuHideNewGame||g_cfg.mainMenuHideMissionSelect){
        u32 mc=0;uptr mf=scan_menu_factory(exe,mc);
        log_cstr("Main menu factory signature matches: ");log_dec(mc);
        log_cstr(" first=");log_hex(mf);log_bytes("\r\n",2);
        if(mc==1&&mf&&install_menu_factory_hook(api,mf))log_line("Main menu factory hook installed.");
        else log_line("Main menu cleanup requested but menu factory hook was not installed.");
    }else{
        log_line("Main menu cleanup disabled by INI.");
    }
    return 1;
}