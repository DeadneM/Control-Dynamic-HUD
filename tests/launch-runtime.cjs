// Exercise the real embedded HUD script with scoped Launch/Slam DOM fixtures.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('src/ControlDynamicHUD.cpp','utf8');
const script=source.match(/static const char kSuiteScript\[\] = R"JS\(([\s\S]*?)\)JS";/)[1];
function el(classes='',attrs={}) {
  const tokens=new Set(classes.split(' ').filter(Boolean)),values={...attrs};
  return {isConnected:true,classList:{contains:x=>tokens.has(x),add:x=>tokens.add(x),remove:x=>tokens.delete(x)},
    getAttribute:n=>values[n]??null,setAttribute:(n,v)=>{values[n]=v;},removeAttribute:n=>{delete values[n];},querySelector(){return this.bg||null;}};
}
function target(model) {const root=el('launch-slam-target launch-slam-target--shown',{'data-bind-style-transform2d':'{{'+model+'.m_strLaunchTargetTransform}}'});root.bg=el('launch-slam-target__background');return root;}
function setup(options={}) {
  let now=100,id=0;const timers=new Map(),observers=[],intervals=[],listeners=[],byId={};
  const one=target('g_multiLaunchIndicator1'),two=target('g_multiLaunchIndicator2'),three=target('g_multiLaunchIndicator3'),slam=target('g_slamIndicator'),unknown=target('unrelated');
  two.removeAttribute('data-bind-style-transform2d');two.bg.setAttribute('data-bind-if','{{g_multiLaunchIndicator2.m_bIsReticuleHidden}} === false');
  three.removeAttribute('data-bind-style-transform2d');three.bg.classList.add('launch-slam-target__background--third');
  const roots=[one,two,three,slam,unknown];const normal=el('awesome-crosshair'),icon=el('launch-object-highlight');
  const ctx={__CDH_NATIVE_CONFIG:{diagnostics:0,launchHideReticle:1,launchHideDelayMs:1200,launchFadeDurationMs:450,showHudDurationMs:5000,...options},
    g_HUDMode:{m_iPlayerMode:0,m_bIsHudVisible:true},
    document:{documentElement:{},head:{appendChild(n){byId[n.id]=n;}},getElementById:id=>byId[id],createElement:()=>el(),querySelector:()=>null,querySelectorAll:s=>s==='.launch-slam-target'?roots:[]},
    engine:{addModelChangeListener:(m,k,cb)=>listeners.push({m,k,cb}),on(){}},
    MutationObserver:class {constructor(cb){this.cb=cb;observers.push(this);}observe(root){this.root=root;}disconnect(){this.root=null;}},
    setTimeout(cb,ms){timers.set(++id,{cb,time:now+ms});return id;},clearTimeout:id=>timers.delete(id),
    setInterval(cb){intervals.push(cb);return intervals.length;},Date:{now:()=>now},addEventListener(){}};
  ctx.window=ctx;vm.createContext(ctx);vm.runInContext(script,ctx);
  function tick(ms){const end=now+ms;for(;;){const due=[...timers].filter(([,t])=>t.time<=end).sort((a,b)=>a[1].time-b[1].time)[0];if(!due)break;now=due[1].time;timers.delete(due[0]);due[1].cb();}now=end;}
  function changed(root){observers.filter(o=>o.root===root).forEach(o=>o.cb());}
  tick(0);return {ctx,roots,one,two,three,slam,unknown,normal,icon,tick,changed,heartbeat:()=>intervals[0](),style:()=>byId['cdh-suite-style'].textContent,listeners};
}
const s=setup();
for(const root of [s.one,s.two,s.three])assert.equal(root.getAttribute('data-cdh-launch-target'),'1');
for(const root of [s.slam,s.unknown,s.normal,s.icon])assert.equal(root.getAttribute('data-cdh-launch-target'),null,'unrelated components stay untouched');
assert(s.style().includes('transition:opacity 450ms'),'configured CSS fade reaches managed targets');
s.tick(1000);s.heartbeat();s.changed(s.one);s.tick(199);assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'0');s.tick(1);
assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'1','combat does not force visibility or postpone timer');
s.ctx.__ControlDynamicHUDSuite.forceShowHUD();assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'0');s.tick(4999);assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'0');s.tick(1);s.tick(1199);assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'0');s.tick(1);assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'1','hide restarts after F1');
s.one.classList.remove('launch-slam-target--shown');s.changed(s.one);assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'0');
s.one.classList.add('launch-slam-target--shown');s.changed(s.one);s.tick(1200);assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'1');
s.ctx.g_HUDMode.m_iPlayerMode=5;s.listeners.find(l=>l.k==='m_iPlayerMode').cb();assert.equal(s.one.getAttribute('data-cdh-launch-target'),null,'suspend restores own attributes');
s.ctx.g_HUDMode.m_iPlayerMode=0;s.listeners.find(l=>l.k==='m_iPlayerMode').cb();s.tick(1350);assert.equal(s.one.getAttribute('data-cdh-launch-hidden'),'1','combat resume rebinds targets');
const replacement=target('g_multiLaunchIndicator1');s.one.isConnected=false;s.roots[0]=replacement;s.heartbeat();s.tick(1200);assert.equal(replacement.getAttribute('data-cdh-launch-hidden'),'1','recreated target is managed');
const disabled=setup({launchHideReticle:0});disabled.tick(10000);disabled.heartbeat();assert.equal(disabled.one.getAttribute('data-cdh-launch-target'),null);
const instant=setup({launchHideDelayMs:0,launchFadeDurationMs:0});assert.equal(instant.one.getAttribute('data-cdh-launch-hidden'),'1');assert(instant.style().includes('transition:opacity 0ms'));
console.log('PASS: Launch-only identity, combat hiding, delay/fade config, idempotent timer, F1, reacquisition, pause/resume, DOM replacement, disabled/instant settings.');
