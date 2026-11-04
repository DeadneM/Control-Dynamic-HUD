// Embedded menu scripts in a mock DOM/engine: does not execute Control/Coherent.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('src/ControlDynamicHUD.cpp', 'utf8');
const script = name => source.match(new RegExp('static const char '+name+'\\[\\] = R"JS\\(([\\s\\S]*?)\\)JS";'))[1];
const pre = script('kMainMenuPreReadyScript'), menu = script('kMainMenuScript');
function setup() {
  const context = {__CDH_MAINMENU_CONFIG:{hideNewGame:1,hideMissionSelect:1},
    document:{body:{querySelectorAll:()=>[]},documentElement:{},activeElement:null,
      head:{appendChild(){}},getElementById:()=>null,createElement:()=>({}),
      addEventListener(){},querySelectorAll:()=>[]},
    MutationObserver:class {observe(){}},setTimeout(){}};
  context.window=context;vm.createContext(context);return context;
}
function engine() {
  return {handlers:{},on(name,cb,ctx){this.handlers[name]={cb,ctx};return 'registered';}};
}
function row(classes,selected='false') {
  return {isConnected:true,getAttribute:n=>n==='class'?classes:n==='aria-selected'?selected:null,
    querySelectorAll:()=>[],contains:()=>false};
}
const c=setup();
vm.runInContext(pre,c);
assert.equal(c.__CDH_PRENAV_INSTALLED,undefined,'missing engine must remain retryable');
c.engine=engine();vm.runInContext(pre,c);
const firstOn=c.engine.on;vm.runInContext(pre,c);assert.equal(c.engine.on,firstOn,'no wrapper stacking');
vm.runInContext(menu,c);
c.__CDH_NEWGAME_ABOVE=row('menu-item--selected');c.__CDH_NEWGAME_BELOW=row('menu-item--unselected');
let count=0;const receiver={};
assert.equal(c.engine.on('OnNavigateDown',function(arg){assert.equal(this,receiver);assert.equal(arg,42);count++;return 17;},receiver),'registered');
assert.equal(c.engine.handlers.OnNavigateDown.cb.call(receiver,42),17);assert.equal(count,2,'selected boundary skips one ghost');
c.__CDH_NEWGAME_ABOVE=row('menu-item--unselected');count=0;c.engine.handlers.OnNavigateDown.cb.call(receiver,42);assert.equal(count,1,'unselected must not match selected');
c.__CDH_NEWGAME_ABOVE=row('menu-item inactive','false');count=0;c.engine.handlers.OnNavigateDown.cb.call(receiver,42);assert.equal(count,1,'inactive/false must not count as selected');
c.__CDH_NEWGAME_BELOW=row('menu-item','true');count=0;c.engine.on('OnNavigateUp',()=>count++);c.engine.handlers.OnNavigateUp.cb();assert.equal(count,2,'reverse boundary skip');
c.engine=engine();vm.runInContext(pre,c);count=0;c.engine.on('OnNavigateUp',()=>count++);c.engine.handlers.OnNavigateUp.cb();assert.equal(count,2,'new engine wrapped on reinjection');
const fresh=setup();fresh.engine=engine();vm.runInContext(pre,fresh);vm.runInContext(menu,fresh);
fresh.__CDH_NEWGAME_ABOVE=row('menu-item focused');count=0;fresh.engine.on('OnNavigateDown',()=>count++);fresh.engine.handlers.OnNavigateDown.cb();assert.equal(count,2,'fresh JS context after recreation');
const off=setup();off.__CDH_MAINMENU_CONFIG={hideNewGame:0,hideMissionSelect:0};vm.runInContext(menu,off);assert.equal(off.__ControlDynamicHUDMainMenu,undefined,'menu defaults stay inactive');
console.log('PASS: menu wrapper retry, deduplication, engine/context recreation, selected states, direction, callback args/receiver/result, disabled defaults.');
