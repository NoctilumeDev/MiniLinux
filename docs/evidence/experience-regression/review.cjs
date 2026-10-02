const fs = require('fs');
const path = require('path');
const childProcess = require('child_process');
const { chromium } = require('C:/Users/lenovo/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const repo = 'C:/Users/lenovo/Desktop/GitHubProjects/MiniLinux';
const revision = 'cc937317c7af9a99d8987059776654f0a85a57d8';
const out = __dirname;
function frozen(file) { return childProcess.execFileSync('git', ['show', revision + ':' + file], {cwd: repo, encoding:'utf8'}); }
const source = new Map([['/', frozen('web/index.html')], ['/app.js',frozen('web/app.js')], ['/style.css',frozen('web/style.css')]]);
const makeEvent = (id,kind='schedule') => ({id,kind,tick:id, fields:kind==='fault'?['3','14','4','0xffff800000000000','0x0000000000400010','3']:kind==='boot'?['LAB','x86_64','100','8']:['3','4','0x0000000000080000','3']});
const snapshot = (epoch,sequence,events,error='') => ({epoch,sequence,events,error,status:'running',console:'mini$ ',console_base:0,guest_memory:64,processes:[{pid:2,parent:1,program:'shell',state:'ready',root:'0x82000',current:true}],maps:{},data:{},frames:16368,free:15976,ticks:sequence});
const snapshots = [snapshot('A',4,[makeEvent(1,'boot'),makeEvent(2),makeEvent(3,'fault'),makeEvent(4)])];
const requests=[];
(async () => {
const browser = await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--no-first-run']});
try {
const page = await browser.newPage({viewport:{width:1366,height:768}});
const errors=[]; page.on('pageerror',error=>errors.push(String(error)));
await page.addInitScript(() => { const nativeSetTimeout=window.setTimeout; window.setTimeout=(callback,delay,...args)=>callback.name==='poll'?0:nativeSetTimeout(callback,delay,...args); });
await page.route('**/*',async route => {
  const url=new URL(route.request().url());
  if(url.pathname==='/api/state') { requests.push(url.pathname+url.search); if(!snapshots.length) throw new Error('No prepared snapshot'); await route.fulfill({status:200,contentType:'application/json',body:JSON.stringify(snapshots.shift())}); return; }
  if(source.has(url.pathname)){await route.fulfill({status:200,contentType:url.pathname.endsWith('.js')?'application/javascript':url.pathname.endsWith('.css')?'text/css':'text/html',body:source.get(url.pathname)});return;}
  await route.fulfill({status:404,body:''});
});
await page.goto('http://minilinux-review.test/');
await page.waitForFunction(()=>eventRows.size===4);
const rowOrder = () => page.evaluate(()=>Array.from(document.getElementById('events').children).filter(el=>el.tagName==='BUTTON').map(el=>Array.from(eventRows).find(([id,node])=>node===el)[0]));
const readDetail = () => page.evaluate(()=>({meta:document.getElementById('event-meta').textContent,fields:document.getElementById('event-fields').textContent,pressed:document.getElementById('follow-events').getAttribute('aria-pressed')}));
const results={revision,tests:{}};
const traceBefore=await rowOrder(); await page.locator('[data-view="fault"]').click(); const fault=await rowOrder(); await page.locator('[data-view="trace"]').click();
results.tests.filterOrder={traceBefore,fault,traceAfter:await rowOrder(),expected:[1,2,3,4],detail:await readDetail()};
await page.screenshot({path:path.join(out,'filter-order-first-failure.png')});
// Test window eviction using the actual source renderer and native browser focus behavior.
await page.evaluate(makeEvents=>{events=makeEvents;eventRows.clear();document.getElementById('events').replaceChildren();selectedEvent=null;eventStart=0;eventDetailKey='';renderEvents();},Array.from({length:80},(_,i)=>makeEvent(i+1)));
await page.evaluate(()=>eventRows.get(1).focus()); await page.keyboard.press('Enter');
const pinnedBefore=await readDetail();
const activeBefore=await page.evaluate(()=>document.activeElement.getAttribute('aria-label'));
await page.evaluate(next=>{events.push(next);renderEvents();},makeEvent(81));
results.tests.focusEviction={activeBefore,pinnedBefore,activeAfter:await page.evaluate(()=>({tag:document.activeElement.tagName,id:document.activeElement.id,label:document.activeElement.getAttribute('aria-label')})),pinnedAfter:await readDetail(),rowIds:await rowOrder()};
await page.keyboard.press('Tab'); results.tests.focusEviction.afterTab=await page.evaluate(()=>({id:document.activeElement.id,label:document.activeElement.getAttribute('aria-label')}));
// Every new epoch must reconstruct its complete prefix even if old cursor is less than new sequence.
await page.evaluate(()=>{cursor=2;epoch='A';});
snapshots.push(snapshot('B',4,[makeEvent(3,'fault'),makeEvent(4)]));
await page.evaluate(()=>poll());
results.tests.epochNonempty={requests:requests.slice(),epoch:await page.evaluate(()=>epoch),cursor:await page.evaluate(()=>cursor),rowIds:await rowOrder(),retainedEventIds:await page.evaluate(()=>events.map(e=>e.id)),detail:await readDetail()};
// Returning latest updates detail but does not restart list scrolling after a pin.
await page.evaluate(makeEvents=>{events=makeEvents;eventRows.clear();document.getElementById('events').replaceChildren();selectedEvent=null;eventStart=0;eventDetailKey='';renderEvents();},Array.from({length:80},(_,i)=>makeEvent(i+1)));
await page.evaluate(()=>{const log=document.getElementById('events');log.scrollTop=0;eventRows.get(1).click();});
await page.locator('#follow-events').click();
results.tests.latestScroll={detail:await readDetail(),geometry:await page.evaluate(()=>{const log=document.getElementById('events');return {top:log.scrollTop,height:log.clientHeight,scrollHeight:log.scrollHeight};})};
// Network error followed by a healthy state leaves the prior alert visible.
await page.route('**/api/state*', route=>route.abort('connectionrefused'));
await page.evaluate(()=>poll());
results.tests.recoveryError={before:await page.evaluate(()=>({connection:document.getElementById('connection').textContent,errorHidden:document.getElementById('error').hidden,error:document.getElementById('error').textContent}))};
await page.unroute('**/api/state*'); snapshots.push(snapshot('B',81,[]));await page.evaluate(()=>poll());
results.tests.recoveryError.after=await page.evaluate(()=>({connection:document.getElementById('connection').textContent,errorHidden:document.getElementById('error').hidden,error:document.getElementById('error').textContent}));
results.pageErrors=errors;fs.writeFileSync(path.join(out,'first-failure-results.json'),JSON.stringify(results,null,2));console.log(JSON.stringify(results,null,2));
} finally { await browser.close(); }
})().catch(error=>{console.error(error);process.exitCode=1;});
