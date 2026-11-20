// Real embedded auto-continue script in a deterministic DOM/engine harness.
// This does not execute Coherent, read a save, or prove loading in Control.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('src/ControlDynamicHUD.cpp','utf8');
const script=source.match(/static const char kAutoContinueScript\[\] = R"JS\(([\s\S]*?)\)JS";/)[1];
const preNav=source.match(/static const char kMainMenuPreReadyScript\[\] = R"JS\(([\s\S]*?)\)JS";/)[1];
function classes(...initial){const set=new Set(initial);return {contains:x=>set.has(x),add:x=>set.add(x),remove:x=>set.delete(x)};}
function setup({enabled=1,delayMs=2000,attached=true,navigationWrapper=false}={}){
 let now=100,next=0;const timers=new Map(),events=new Map(),inputs=new Map(),requests=[];
 const root={isConnected:true,classList:classes('main-menu-root'),getBoundingClientRect:()=>({width:400,height:500})};
 const dom={body:{classList:classes('main-menu')},pause:false,root,css:{display:'block',visibility:'visible',opacity:'1'},
  querySelector:s=>s==='.main-menu-root'?dom.root:s==='.app--pause-menu'&&dom.pause?{}:null,
  addEventListener(n,cb){if(!inputs.has(n))inputs.set(n,new Set());inputs.get(n).add(cb);},
  removeEventListener(n,cb){inputs.get(n)?.delete(cb);}};
 const engine={isAttached:attached,on(n,cb){if(!events.has(n))events.set(n,new Set());events.get(n).add(cb);},off(n,cb){events.get(n)?.delete(cb);},
  trigger(n,...args){requests.push(n);for(const cb of [...(events.get(n)||[])])cb(...args);}};
 const ctx={__CDH_AUTOCONTINUE_CONFIG:{enabled,delayMs},engine,document:dom,getComputedStyle:()=>dom.css,
  Date:{now:()=>now},setTimeout(cb,ms){timers.set(++next,{cb,time:now+ms});return next;},clearTimeout:id=>timers.delete(id)};
 ctx.window=ctx;vm.createContext(ctx);if(navigationWrapper)vm.runInContext(preNav,ctx);vm.runInContext(script,ctx);
 function tick(ms){const end=now+ms;for(;;){const due=[...timers].filter(([,t])=>t.time<=end).sort((a,b)=>a[1].time-b[1].time)[0];if(!due)break;now=due[1].time;timers.delete(due[0]);due[1].cb();}now=end;}
 const emit=(n,...args)=>{for(const cb of [...(events.get(n)||[])])cb(...args);};
 const show=overrides=>emit('ShowMenu',{m_bIsPauseMenu:false,m_bSaveAvailable:true,m_bIsDemo:false,m_bShouldShowEula:false,m_bShouldShowPrivacy:false,m_bShouldShowDLCPopup:false,...overrides});
 const input=n=>{for(const cb of [...(inputs.get(n)||[])])cb({});};
 return {ctx,dom,root,engine,requests,timers,events,inputs,tick,emit,show,input,rerun:()=>vm.runInContext(script,ctx)};
}
function idle(s){assert.equal(s.requests.length,0);assert.equal(s.timers.size,0);}
const off=setup({enabled:0});off.show();off.tick(60000);idle(off);assert.equal(off.ctx.__CDH_AUTO_CONTINUE,undefined);assert.equal(off.events.size,0);
const ok=setup();let nativeContinue=0;ok.engine.on('OnContinueGameClicked',()=>nativeContinue++);ok.tick(500);assert.equal(ok.requests.length,0,'wait for actual ShowMenu payload');ok.show();ok.tick(2099);assert.equal(ok.requests.length,0);ok.tick(1);assert.deepEqual(ok.requests,['OnContinueGameClicked']);assert.equal(nativeContinue,1,'use the normal Continue action');assert.equal(ok.ctx.__CDH_AUTO_CONTINUE.status,'requested');assert.equal(ok.timers.size,0);ok.rerun();ok.show();ok.tick(60000);assert.equal(nativeContinue,1,'no retry on reinjection or return to menu');
for(const options of [{m_bSaveAvailable:false},{m_bSaveAvailable:undefined},{m_bIsPauseMenu:true},{m_bIsPauseMenu:undefined},{m_bIsDemo:true},{m_bShouldShowEula:true},{m_bShouldShowPrivacy:true},{m_bShouldShowDLCPopup:true}]){
 const s=setup();s.show(options);s.tick(60000);idle(s);s.show();s.tick(60000);idle(s);
}
const missing=setup();missing.emit('ShowMenu',null);missing.tick(60000);idle(missing);
for(const name of ['keydown','mousedown','wheel','touchstart']){const s=setup();s.show();s.tick(500);s.input(name);s.tick(60000);idle(s);assert.equal(s.ctx.__CDH_AUTO_CONTINUE.status,'cancelled-input');}
for(const name of ['OnNavigateDown','OnNavigateUp','OnNavigateLeft','OnNavigateRight','OnSelect','OnCancel','OnStart','OnOptions','OnPrev','OnNext','OnX','OnY']){
 const s=setup();let normalInput=0;s.engine.on(name,()=>normalInput++);s.show();s.tick(500);s.emit(name);s.tick(60000);idle(s);assert.equal(normalInput,1,'cancellation preserves manual input callbacks');
}
const twice=setup();twice.show();twice.tick(500);twice.show();twice.tick(60000);idle(twice);
const hidden=setup();hidden.show();hidden.tick(500);hidden.root.classList.add('main-menu-root--hidden');hidden.tick(100);hidden.root.classList.remove('main-menu-root--hidden');hidden.tick(60000);idle(hidden);
for(const place of ['body','root','pause']){const s=setup();s.show();if(place==='pause')s.dom.pause=true;else (place==='body'?s.dom.body:s.root).classList.add('confirm-box--active');s.tick(100);idle(s);assert.equal(s.ctx.__CDH_AUTO_CONTINUE.status,'blocked-menu');}
const slow=setup();slow.root.classList.add('main-menu-root--hidden');slow.show();slow.tick(1000);assert.equal(slow.requests.length,0);slow.root.classList.remove('main-menu-root--hidden');slow.tick(2100);assert.deepEqual(slow.requests,['OnContinueGameClicked']);
const timeout=setup();timeout.show();timeout.dom.root=null;timeout.tick(60000);idle(timeout);assert.equal(timeout.ctx.__CDH_AUTO_CONTINUE.status,'timeout');
const noPayload=setup();noPayload.tick(60000);idle(noPayload);
const cancel=setup();cancel.show();cancel.tick(100);cancel.ctx.__CDH_CANCEL_AUTO_CONTINUE();cancel.tick(60000);idle(cancel);
const replaced=setup();replaced.show();replaced.ctx.engine={};replaced.tick(100);idle(replaced);assert.equal(replaced.ctx.__CDH_AUTO_CONTINUE.status,'cancelled-engine-change');
const detached=setup({attached:false});detached.tick(60000);idle(detached);
const throws=setup({delayMs:0});throws.engine.trigger=()=>{throw Error('native dispatch failed');};throws.show();throws.tick(100);assert.equal(throws.ctx.__CDH_AUTO_CONTINUE.status,'trigger-error');assert.equal(throws.timers.size,0);
const wrapped=setup({navigationWrapper:true});let normalMoves=0;wrapped.ctx.__CDH_SHOULD_DOUBLE_NAV=()=>true;wrapped.engine.on('OnNavigateDown',()=>normalMoves++);wrapped.show();wrapped.emit('OnNavigateDown');wrapped.tick(60000);idle(wrapped);assert.equal(normalMoves,2,'validated ghost skip still runs for the normal handler');assert.equal(wrapped.events.get('OnNavigateDown').size,1,'only normal handler survives; cancellation callback removed');assert.equal(wrapped.events.get('OnNavigateUp').size,0,'no orphaned navigation wrapper for auto cancellation');
for(const s of [ok,twice,hidden,slow,timeout,cancel,replaced]){
 assert(!s.requests.some(x=>x!=='OnContinueGameClicked'),'never call New Game, Mission Select, or generic Select');
 for(const subscribers of s.inputs.values())assert.equal(subscribers.size,0,'input observers removed after completion');
}
console.log('PASS: default off, actual Continue dispatch once, save/pause/dialog gates, input cancellation, render readiness, context cancellation, timeout, failure without retry, listener cleanup.');
