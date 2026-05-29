import { readFileSync } from 'node:fs';
import { test } from 'vitest';

function parseRx(line: string): Uint8Array | null {
  const parts=line.trim().split(/\s+/); if(parts[1]!== 'RX') return null;
  const b64=parts[3] ?? ''; const buf=Buffer.from(b64,'base64');
  return new Uint8Array(buf.buffer,buf.byteOffset,buf.byteLength);
}

test('dump target cell full', async () => {
  if (!('localStorage' in globalThis)) {
    const store = new Map<string, string>();
    Object.defineProperty(globalThis, 'localStorage', { value: { getItem:(k:string)=>store.get(String(k))??null,setItem:(k:string,v:string)=>store.set(String(k),String(v)),removeItem:(k:string)=>store.delete(String(k)),clear:()=>store.clear(),key:(i:number)=>Array.from(store.keys())[i]??null,get length(){return store.size;} } });
  }
  const u = URL as any; u.createObjectURL ??= ()=>'blob:test'; u.revokeObjectURL ??= ()=>{};
  (globalThis as any).Audio ??= class { play(){return Promise.resolve();} pause(){} };
  const { dispatchPacket } = await import('../src/lib/commands');
  const { clientInit } = await import('../src/lib/init');
  const { getFaceTileSize } = await import('../src/lib/image');
  const { setGetMapImageSize, mapdata_cell, pl_mpos } = await import('../src/lib/mapdata');
  const { initCommands } = await import('../src/lib/p_cmd');
  const { resetReplaySandboxState, toPacketBuffer } = await import('../src/lib/replay');
  clientInit(); initCommands(); setGetMapImageSize(getFaceTileSize); resetReplaySandboxState();

  const lines=readFileSync('/tmp/workspace/bofh69/crossfire-web-client/tests/replay-mapdata/logs/zoo.log','utf8').split(/\r?\n/);
  let absX=0,absY=0;
  for(let i=0;i<1830;i++){
    const rx=parseRx(lines[i] ?? '');
    if(rx) dispatchPacket(toPacketBuffer(rx));
    if(i===1827){ const p=pl_mpos(); absX=p.px-4; absY=p.py-1; }
    if(i===1827||i===1828){
      const p=pl_mpos();
      const c=mapdata_cell(absX,absY);
      const heads=c.heads.map((h,j)=>({layer:j,face:h.face,sizeX:h.sizeX,sizeY:h.sizeY,anim:h.animation,animSpeed:h.animationSpeed})).filter(h=>h.face!==0||h.sizeX!==1||h.sizeY!==1||h.anim!==0||h.animSpeed!==0);
      const tails=c.tails.map((t,j)=>({layer:j,face:t.face,sizeX:t.sizeX,sizeY:t.sizeY})).filter(t=>t.face!==0||t.sizeX!==0||t.sizeY!==0);
      const smooth=c.smooth.map((v,j)=>({layer:j,value:v})).filter(s=>s.value!==0);
      console.log('after line',i+1,'player',p,'rel',{dx:absX-p.px,dy:absY-p.py},'state',c.state,'dark',c.darkness,'heads',JSON.stringify(heads),'tails',JSON.stringify(tails),'smooth',JSON.stringify(smooth));
    }
  }
});
