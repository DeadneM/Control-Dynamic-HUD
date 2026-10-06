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

typedef void* (WINAPI *FactoryFn)(void*, void*, void*, void*);
typedef void* (WINAPI *PageGetViewFn)(void*);
typedef void (WINAPI *ExecuteScriptFn)(void*, const char*, const char*);
typedef void (WINAPI *ReadyFn)(void*);
typedef void (WINAPI *UpdateFn)(void*);

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
static u32 g_showHudVk = 0;
static bool g_showHudKeyDown = false;
static volatile u32 g_factoryCalls = 0;
static volatile u32 g_readyCalls = 0;

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

    bool expeditionEnabled;
    u32 expeditionHideDelayMs;
    u32 expeditionFadeDurationMs;

    bool hideGroundSlamTargetCircle;

    bool multiLaunchHideInputPrompts;
    u32 multiLaunchHideDelayMs;
    u32 multiLaunchFadeDurationMs;
};
static Config g_cfg{};
static char g_configScript[2048];

static constexpr u32 kCoherentTimeDateStamp = 0x5E8F6E9A;
static constexpr u32 kCoherentSizeOfImage = 0x329000;
static constexpr u32 kCoherentCheckSum = 0x00323A4A;
static constexpr uptr kPublicViewVtableRva = 0x270780;
static constexpr uptr kExecuteScriptRva = 0x82870;
static constexpr uptr kViewPageOffset = 0xA8;

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
  expeditionEnabled:1,expeditionHideDelayMs:2000,expeditionFadeDurationMs:300,
  multiLaunchHideInputPrompts:1,multiLaunchHideDelayMs:0,multiLaunchFadeDurationMs:150
 };
 var CDH=window.__ControlDynamicHUDSuite={
  version:'1.0B',health:false,mission:false,crosshair:false,expedition:false,
  hudVisible:true,active:true,lastError:'',forceVisible:false
 };
 var MODE={COMBAT:0,ADVENTURING:1,STORY:2,ACTION:3,EXAMINE:4,HIDDEN:5};
 var playerMode=1,isAiming=false,hudVisible=true,active=true;
 var hp={bar:null,fill:null,obs:null,timer:0,shown:true};
 var mission={map:null,log:null,obs:null,timer:0,shown:true};
 var cross={el:null,timer:0,shown:true,hideLatched:false};
 var expedition={el:null,timer:0,shown:true};
 var multiLaunch={timer:0,tagged:[],bindingMatches:0};
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
  +'.expedition-hud>.expedition-mod-group{transition:opacity '+CFG.expeditionFadeDurationMs+'ms var(--easing);}'
  +'.expedition-hud>.expedition-mod-group[data-cdh-expedition-hidden="1"]{opacity:0!important;}'
  +'[data-cdh-multilaunch-input-hidden="1"]{opacity:0!important;visibility:hidden!important;transition:opacity '+CFG.multiLaunchFadeDurationMs+'ms var(--easing);}'
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

 function multiLaunchModels(){
  var a=[];
  if(window.g_multiLaunchIndicator1)a.push(g_multiLaunchIndicator1);
  if(window.g_multiLaunchIndicator2)a.push(g_multiLaunchIndicator2);
  if(window.g_multiLaunchIndicator3)a.push(g_multiLaunchIndicator3);
  return a;
 }
 function multiLaunchActive(){
  var a=multiLaunchModels();
  for(var i=0;i<a.length;i++){
   if(a[i]&&a[i].m_bHighlightVisible!==false)return true;
  }
  return false;
 }
 function clearMultiLaunchTags(){
  clearTimer(multiLaunch);
  for(var i=0;i<multiLaunch.tagged.length;i++){
   var e=multiLaunch.tagged[i];
   if(e&&e.isConnected)e.removeAttribute('data-cdh-multilaunch-input-hidden');
  }
  multiLaunch.tagged=[];
  multiLaunch.bindingMatches=0;
 }
 function bindingText(e){
  if(!e||!e.attributes)return '';
  var out='';
  for(var i=0;i<e.attributes.length;i++){
   var a=e.attributes[i];
   out+=' '+String(a.name||'')+'='+String(a.value||'');
  }
  return out.toLowerCase();
 }
 function isInteractionButtonBinding(e){
  var t=bindingText(e);
  if(t.indexOf('m_fbuttonopacity')!==-1)return true;
  if(t.indexOf('buttonopacity')!==-1)return true;
  var c=(String(e.className||'')+' '+String(e.id||'')).toLowerCase();
  return /interaction[^ ]*(button|prompt)|(button|prompt)[^ ]*interaction/.test(c);
 }
 function applyMultiLaunchPromptHide(){
  clearMultiLaunchTags();
  if(!active||!CFG.multiLaunchHideInputPrompts||isForced()||!multiLaunchActive())return;

  var all=document.querySelectorAll('*');
  for(var i=0;i<all.length;i++){
   var e=all[i];
   if(!isInteractionButtonBinding(e))continue;
   e.setAttribute('data-cdh-multilaunch-input-hidden','1');
   multiLaunch.tagged.push(e);
  }
  multiLaunch.bindingMatches=multiLaunch.tagged.length;
 }
 function updateMultiLaunchPrompts(){
  if(!CFG.multiLaunchHideInputPrompts||!multiLaunchActive()){
   clearMultiLaunchTags();
   return;
  }
  if(isForced()){
   clearMultiLaunchTags();
   return;
  }
  clearTimer(multiLaunch);
  if(CFG.multiLaunchHideDelayMs===0){
   applyMultiLaunchPromptHide();
   return;
  }
  multiLaunch.timer=setTimeout(function(){
   multiLaunch.timer=0;
   if(active&&!isForced()&&multiLaunchActive())applyMultiLaunchPromptHide();
  },CFG.multiLaunchHideDelayMs);
 }


 function rebind(){
  rebindTimer=0;
  if(!active)return;
  addStyle();
  bindHealth();bindMission();bindCrosshair();bindExpedition();updateMultiLaunchPrompts();updateDiag();
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
  clearTimer(hp);clearTimer(mission);clearTimer(cross);clearTimer(expedition);clearTimer(multiLaunch);
  disconnectObserver(hp);disconnectObserver(mission);
  hp.bar=null;hp.fill=null;mission.map=null;mission.log=null;cross.el=null;expedition.el=null;
  CDH.health=false;CDH.mission=false;CDH.crosshair=false;CDH.expedition=false;
  clearMultiLaunchTags();
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
   (CFG.expeditionEnabled&&(!expedition.el||!expedition.el.isConnected));
  if(needs)scheduleRebind(0);
  if(CFG.multiLaunchHideInputPrompts&&multiLaunchActive()&&!isForced())applyMultiLaunchPromptHide();
 }

 function forceShowHUD(){
  if(!active)return;
  forceUntil=Date.now()+CFG.showHudDurationMs;CDH.forceVisible=true;
  clearTimer(hp);clearTimer(mission);clearTimer(cross);clearTimer(expedition);clearTimer(multiLaunch);
  clearMultiLaunchTags();
  if(hp.bar)setHpHidden(false);
  if(mission.log)missionShow();
  if(cross.el)setCrossHidden(false);
  if(expedition.el)setExpeditionHidden(false);
  if(forceTimer)clearTimeout(forceTimer);
  forceTimer=setTimeout(function(){
   forceTimer=0;forceUntil=0;CDH.forceVisible=false;
   if(!active)return;
   cross.hideLatched=false;
   updateHealth();onMapChanged();updateCrosshair();scheduleExpeditionHide();updateMultiLaunchPrompts();
  },CFG.showHudDurationMs);
  updateDiag();
 }
 CDH.forceShowHUD=forceShowHUD;

 function yes(v){return v?'YES':'no';}
 function updateDiag(){
  if(!CFG.diagnostics)return;
  var p=document.getElementById('cdh-diagnostic');if(!p)return;
  p.textContent=
   'Control Dynamic HUD v1.0G configurable suite\n'
  +'HUD '+yes(hudVisible)+' | active '+yes(active)+' | force '+yes(isForced())+' | aim '+yes(isAiming)+' | key '+CFG.showHudKey+'\n'
  +'health '+yes(CDH.health)+' | mission '+yes(CDH.mission)+' | crosshair '+yes(CDH.crosshair)+' | expedition '+yes(CDH.expedition)+' | multiLaunch '+yes(multiLaunchActive())+' bindings '+multiLaunch.bindingMatches+'\n'
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
   if(active)updateCrosshair();
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
  var ml=multiLaunchModels();
  for(var mi=0;mi<ml.length;mi++){
   (function(model){
    engine.addModelChangeListener(model,'m_bHighlightVisible',function(){
     updateMultiLaunchPrompts();updateDiag();
    });
    engine.addModelChangeListener(model,'m_strHighlightObjectTransform',function(){
     updateMultiLaunchPrompts();updateDiag();
    });
   })(ml[mi]);
  }
  startDiag();
  heartbeat=setInterval(heartbeatTick,1000);
  if(active)rebindBurst();
 }
 boot();
})();
)JS";

static inline void* get_peb() { void* peb; __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(peb)); return peb; }
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

struct WinApi { FnCreateFileW CreateFileW; FnWriteFile WriteFile; FnCloseHandle CloseHandle; FnVirtualAlloc VirtualAlloc; FnVirtualProtect VirtualProtect; FnFlushInstructionCache FlushInstructionCache; FnGetPrivateProfileIntA GetPrivateProfileIntA; FnGetPrivateProfileStringA GetPrivateProfileStringA; FnGetAsyncKeyState GetAsyncKeyState; };
static bool init_api(WinApi& a){
    void* k=find_module("kernel32.dll"); if(!k)return false;
    a.CreateFileW=(FnCreateFileW)resolve_export(k,"CreateFileW"); a.WriteFile=(FnWriteFile)resolve_export(k,"WriteFile"); a.CloseHandle=(FnCloseHandle)resolve_export(k,"CloseHandle");
    a.VirtualAlloc=(FnVirtualAlloc)resolve_export(k,"VirtualAlloc"); a.VirtualProtect=(FnVirtualProtect)resolve_export(k,"VirtualProtect"); a.FlushInstructionCache=(FnFlushInstructionCache)resolve_export(k,"FlushInstructionCache");
    a.GetPrivateProfileIntA=(FnGetPrivateProfileIntA)resolve_export(k,"GetPrivateProfileIntA");
    a.GetPrivateProfileStringA=(FnGetPrivateProfileStringA)resolve_export(k,"GetPrivateProfileStringA");
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

    g_cfg.expeditionEnabled=a.GetPrivateProfileIntA("Expedition","Enabled",1,ini)!=0;
    g_cfg.expeditionHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("Expedition","HideDelayMs",2000,ini),0,60000);
    g_cfg.expeditionFadeDurationMs=clamp_u32(a.GetPrivateProfileIntA("Expedition","FadeDurationMs",300,ini),0,10000);

    g_cfg.hideGroundSlamTargetCircle=a.GetPrivateProfileIntA("GroundSlam","HideTargetCircle",1,ini)!=0;

    g_cfg.multiLaunchHideInputPrompts=a.GetPrivateProfileIntA("MultiLaunch","HideInputPrompts",1,ini)!=0;
    g_cfg.multiLaunchHideDelayMs=clamp_u32(a.GetPrivateProfileIntA("MultiLaunch","HideDelayMs",0,ini),0,60000);
    g_cfg.multiLaunchFadeDurationMs=clamp_u32(a.GetPrivateProfileIntA("MultiLaunch","FadeDurationMs",150,ini),0,10000);
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
    p=app(p,end,",expeditionEnabled:");p=app_u32(p,end,g_cfg.expeditionEnabled?1:0);
    p=app(p,end,",expeditionHideDelayMs:");p=app_u32(p,end,g_cfg.expeditionHideDelayMs);
    p=app(p,end,",expeditionFadeDurationMs:");p=app_u32(p,end,g_cfg.expeditionFadeDurationMs);
    p=app(p,end,",multiLaunchHideInputPrompts:");p=app_u32(p,end,g_cfg.multiLaunchHideInputPrompts?1:0);
    p=app(p,end,",multiLaunchHideDelayMs:");p=app_u32(p,end,g_cfg.multiLaunchHideDelayMs);
    p=app(p,end,",multiLaunchFadeDurationMs:");p=app_u32(p,end,g_cfg.multiLaunchFadeDurationMs);
    p=app(p,end,"};");
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

static void WINAPI HookUpdate(void* self){
    if(g_originalUpdate) g_originalUpdate(self);
    if(!g_GetAsyncKeyState||!g_showHudVk||!g_pageGetView)return;

    bool down=(g_GetAsyncKeyState((int)g_showHudVk)&0x8000)!=0;
    if(!down){g_showHudKeyDown=false;return;}
    if(g_showHudKeyDown)return;
    g_showHudKeyDown=true;

    void* view=self?g_pageGetView(self):nullptr;
    uptr viewVtable=view?rdptr(view):0;
    uptr execute=viewVtable?rdptr((u8*)viewVtable+61u*sizeof(uptr)):0;
    uptr expectedVtable=(uptr)g_coherentBase+kPublicViewVtableRva;
    uptr expected=(uptr)g_coherentBase+kExecuteScriptRva;
    if(!view||viewVtable!=expectedVtable||execute!=expected){
        log_line("Show HUD hotkey pressed but current HUD View is not ready; ignored.");
        return;
    }

    log_cstr("Show HUD hotkey pressed: ");log_cstr(g_cfg.showHudKey);log_bytes("\r\n",2);
    ((ExecuteScriptFn)execute)(view,kForceHudScript,nullptr);
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

extern "C" __declspec(dllexport) int WINAPI CDH_Version(){return 106;}
extern "C" __declspec(dllexport) void* CDH_RelocAnchor=(void*)&CDH_Version;

extern "C" BOOL WINAPI DllMain(HMODULE,DWORD reason,LPVOID){
    if(reason!=DLL_PROCESS_ATTACH)return 1; WinApi api{};if(!init_api(api))return 1;
    g_VirtualProtect=api.VirtualProtect;
    g_FlushInstructionCache=api.FlushInstructionCache;
    g_GetAsyncKeyState=api.GetAsyncKeyState;
    load_config(api);g_showHudVk=parse_vk(g_cfg.showHudKey);build_config_script();
    g_WriteFile=api.WriteFile;g_log=api.CreateFileW(L"plugins\\ControlDynamicHUD.log",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!g_log||(uptr)g_log==INVALID_HANDLE_VALUE_U)return 1;
    log_line("Control Dynamic HUD V1.0G MULTI-LAUNCH BUTTON BINDING TEST");log_line("Mode: validated crosshair + native hotkey + m_fButtonOpacity binding suppression during Multi Launch + Ground Slam target suppression");
    log_cstr("Config: Enabled=");log_dec(g_cfg.enabled?1:0);log_cstr(" ShowHUDKey=");log_cstr(g_cfg.showHudKey);log_cstr(" VK=");log_hex(g_showHudVk);log_cstr(" ShowHUDDurationMs=");log_dec(g_cfg.showHudDurationMs);log_bytes("\r\n",2);
    if(!g_cfg.enabled){log_line("Mod disabled by INI; no hook installed.");return 1;}
    log_cstr("Executable: ");log_wide(first_module_name());log_bytes("\r\n",2);void* exe=first_module_base();log_cstr("Executable base: ");log_hex((uptr)exe);log_bytes("\r\n",2);
    patch_ground_slam_target_circle(exe);
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
    log_line("HUD factory hook installed.");return 1;
}