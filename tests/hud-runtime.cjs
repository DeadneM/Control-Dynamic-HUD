// Deterministic embedded-script regression checks; not a Coherent/game test.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('src/ControlDynamicHUD.cpp', 'utf8');
const script = source.match(/static const char kSuiteScript\[\] = R"JS\(([\s\S]*?)\)JS";/)[1];
new vm.Script(script);
new vm.Script(source.match(/static const char kMainMenuScript\[\] = R"JS\(([\s\S]*?)\)JS";/)[1]);
let now = 0, next = 0;
const timers = new Map(), intervals = [], listeners = [], nodes = new Map();
function element() {
  const classes = new Set();
  return {isConnected:true, classList:{contains:x=>classes.has(x),add:x=>classes.add(x),remove:x=>classes.delete(x),toggle(x,v){v?classes.add(x):classes.delete(x);}},setAttribute(){},removeAttribute(){}};
}
let mapChanged;
const map = element(), log = element();
nodes.set('.map-overlay', map); nodes.set('.mission-log-wrapper', log);
const context = {
  __CDH_NATIVE_CONFIG:{diagnostics:0,missionEnabled:1,missionInitialHideDelayMs:2000,missionAfterMapCloseHideDelayMs:3000,missionUpdateVisibleMs:7000,missionShowInMap:1,showHudDurationMs:5000},
  document:{documentElement:{appendChild(){}},head:{appendChild(n){nodes.set(n.id,n);}},getElementById:id=>nodes.get(id),createElement:element,querySelector:s=>nodes.get(s)||null},
  engine:{addModelChangeListener:(m,k,cb)=>listeners.push({m,k,cb}),on(){}},
  MutationObserver:class {constructor(cb){mapChanged=cb;}observe(){}disconnect(){}},
  setTimeout(cb,ms){timers.set(++next,{cb,time:now+ms});return next;},
  clearTimeout:id=>timers.delete(id),setInterval(cb){intervals.push(cb);return intervals.length;},
  Date:{now:()=>now},addEventListener(){}
};
context.window=context;
vm.createContext(context); vm.runInContext(script, context);
function tick(ms){const end=now+ms;while(true){const due=[...timers].filter(([,t])=>t.time<=end).sort((a,b)=>a[1].time-b[1].time)[0];if(!due)break;now=due[1].time;timers.delete(due[0]);due[1].cb();}now=end;}
tick(1000);
assert.equal(context.__ControlDynamicHUDSuite.mission,true,'DOM boots before HUD model');
intervals[0]();tick(1000);
assert(log.classList.contains('cdh-hide'),'rebind burst/heartbeat must not postpone 2s hide');
context.g_HUDMode={m_iPlayerMode:5,m_bIsHudVisible:false};intervals[0]();
assert.equal(context.__ControlDynamicHUDSuite.active,false,'late hidden model suspends HUD');
context.g_missionPromptUIData={};intervals[0]();
assert(listeners.some(l=>l.m===context.g_missionPromptUIData),'mission model binds while inactive');
context.g_HUDMode.m_iPlayerMode=1;context.g_HUDMode.m_bIsHudVisible=true;
listeners.find(l=>l.k==='m_iPlayerMode').cb();tick(2200);
assert(log.classList.contains('cdh-hide'),'resume rebind restores timer');
map.classList.add('map--show');mapChanged();tick(4000);assert(!log.classList.contains('cdh-hide'));
map.classList.remove('map--show');mapChanged();tick(2999);assert(!log.classList.contains('cdh-hide'));tick(1);assert(log.classList.contains('cdh-hide'));
listeners.find(l=>l.k==='m_missionUIData').cb();
assert(!log.classList.contains('cdh-hide'));tick(6999);assert(!log.classList.contains('cdh-hide'));tick(1);assert(log.classList.contains('cdh-hide'));
context.__ControlDynamicHUDSuite.forceShowHUD();assert(!log.classList.contains('cdh-hide'));tick(4999);assert(!log.classList.contains('cdh-hide'));
intervals[0]();intervals[0]();assert.equal(listeners.length,3,'listeners are not duplicated');
assert(!nodes.has('cdh-diagnostic'),'diagnostics stay off');
console.log('PASS: script syntax, late model binding, inactive recovery, idempotent mission timers, 7s mission updates, F1 visibility, listener deduplication, diagnostics off.');
