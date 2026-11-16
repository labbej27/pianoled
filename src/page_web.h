// page_web.h — interface web (page unique, sans dépendance externe), servie depuis la flash
#pragma once

static const char PAGE_WEB[] = R"PAGE(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Piano LED</title>
<style>
:root{--fond:#f4f2ee;--carte:#fff;--texte:#1d1d1f;--doux:#6b6b70;--trait:#ddd9d2;--accent:#2f6fde;--ok:#1f9d55;--err:#d93636;--champ:#f7f6f3}
@media (prefers-color-scheme:dark){:root{--fond:#141416;--carte:#1f1f23;--texte:#ececf0;--doux:#9a9aa3;--trait:#33333a;--accent:#6c9cff;--ok:#3ccf7a;--err:#ff6b6b;--champ:#2a2a30}}
*{box-sizing:border-box}
body{margin:0;background:var(--fond);color:var(--texte);font:16px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif}
header{display:flex;align-items:center;gap:10px;padding:14px 16px 4px;max-width:720px;margin:auto}
h1{font-size:20px;margin:0;flex:1}
#lien{width:10px;height:10px;border-radius:50%;background:var(--err)}
#lien.ok{background:var(--ok)}
main{max-width:720px;margin:auto;padding:8px 16px 40px}
section{background:var(--carte);border:1px solid var(--trait);border-radius:14px;padding:14px;margin:12px 0}
h2{font-size:15px;margin:0 0 10px;color:var(--doux);font-weight:600;text-transform:uppercase;letter-spacing:.04em}
.ligne{display:flex;flex-wrap:wrap;align-items:center;gap:8px;margin:8px 0}
.ligne label{flex:1;min-width:120px}
button,select,input[type=number],input[type=text]{font:inherit;color:var(--texte);background:var(--champ);border:1px solid var(--trait);border-radius:10px;padding:8px 12px;min-height:40px}
button{cursor:pointer}
button.prim{background:var(--accent);border-color:var(--accent);color:#fff}
button.actif{outline:2px solid var(--accent)}
button:disabled{opacity:.45;cursor:default}
input[type=number]{width:110px}
input[type=range]{flex:1;min-width:140px;accent-color:var(--accent)}
.gros button{font-size:22px;min-width:64px}
.seg{display:inline-flex;border:1px solid var(--trait);border-radius:10px;overflow:hidden}
.seg button{border:0;border-radius:0;background:var(--champ)}
.seg button.on{background:var(--accent);color:#fff}
#titre{font-weight:600;word-break:break-all}
.doux{color:var(--doux);font-size:14px}
.chiffres{font-variant-numeric:tabular-nums}
#fichiers .f{display:flex;align-items:center;gap:8px;padding:8px 0;border-top:1px solid var(--trait)}
#fichiers .f span{flex:1;word-break:break-all}
#fichiers .f.charge span{font-weight:600}
.groupe{display:flex;flex-wrap:wrap;align-items:center;gap:8px;padding:8px 0;border-top:1px solid var(--trait)}
.groupe div{flex:1;min-width:160px}
#notes{min-height:1.4em}
.stat{display:inline-block;min-width:90px}
.stat b{font-size:20px}
#erreurs b{color:var(--err)}#justes b{color:var(--ok)}
#toast{position:fixed;left:50%;bottom:16px;transform:translateX(-50%);max-width:calc(100% - 32px);background:var(--texte);color:var(--carte);padding:10px 16px;border-radius:10px;opacity:0;transition:opacity .3s;pointer-events:none}
#toast.vu{opacity:.95}
#toast.err{background:var(--err);color:#fff}
details summary{cursor:pointer;color:var(--doux);font-weight:600;text-transform:uppercase;font-size:15px;letter-spacing:.04em}
.cache{display:none}
</style>
</head>
<body>
<header><h1>Piano LED</h1><span id="lien" title="Connexion"></span></header>
<main>

<section>
  <h2>Lecture</h2>
  <div id="titre">Aucun morceau chargé</div>
  <div class="ligne chiffres"><span id="tpos">0:00</span><input id="pos" type="range" min="0" max="1" value="0" step="100"><span id="tdur">0:00</span></div>
  <div class="ligne gros">
    <button id="bstop" title="Arrêt">⏹</button>
    <button id="blecture" class="prim" title="Lecture">▶</button>
    <button id="bpause" title="Pause">⏸</button>
    <span id="etat" class="doux"></span>
  </div>
  <div class="ligne">
    <span class="seg" id="mode"><button data-v="demo">Démo</button><button data-v="attente">Attente</button></span>
    <span class="seg cache" id="travail"><button data-v="0">2 mains</button><button data-v="1">Droite</button><button data-v="2">Gauche</button></span>
  </div>
  <div class="ligne">
    <label style="flex:0 0 auto"><input type="checkbox" id="mout"> 🔊 Son sur le MX49</label>
  </div>
  <div class="ligne">
    <button id="boctm">Octave −</button><button id="boctp">Octave +</button>
    <span id="toct" class="doux chiffres"></span>
  </div>
  <div class="ligne"><label for="vitesse">Vitesse <b id="tvitesse" class="chiffres">100 %</b> <span id="bpm" class="doux chiffres"></span></label>
    <input id="vitesse" type="range" min="25" max="150" step="5" value="100"></div>
  <div class="ligne">
    <button id="ba">A</button><button id="bb">B</button>
    <label style="flex:0 0 auto"><input type="checkbox" id="boucle"> boucle</label>
    <button id="bab">Effacer A-B</button>
    <span id="tab" class="doux chiffres"></span>
  </div>
  <div class="ligne" id="stats"><span class="stat" id="justes">Justes <b>0</b></span><span class="stat" id="erreurs">Erreurs <b>0</b></span></div>
  <div class="doux">Notes en cours : <span id="notes"></span></div>
</section>

<section>
  <h2>Morceaux</h2>
  <div class="ligne"><input type="file" id="choix" accept=".mid,.midi,audio/midi" multiple><button id="benvoi">Envoyer</button></div>
  <div id="fichiers"></div>
  <div id="place" class="doux"></div>
</section>

<section id="smains" class="cache">
  <h2>Mains</h2>
  <div class="ligne">
    <label>Main droite <input type="color" id="cmd"></label>
    <label>Main gauche <input type="color" id="cmg"></label>
  </div>
  <div class="ligne"><label for="partage">Partage « par hauteur » : main droite à partir de</label><select id="partage"></select></div>
  <div id="groupes"></div>
</section>

<section>
  <h2>Options</h2>
  <div class="ligne"><label for="antic">Anticipation <b id="tantic" class="chiffres"></b></label><input id="antic" type="range" min="0" max="1500" step="50"></div>
  <div class="ligne"><label for="horsplage">Notes hors du clavier</label>
    <select id="horsplage"><option value="1">transposer d'une octave</option><option value="0">ignorer</option></select></div>
</section>

<section>
  <details>
    <summary>Calibration</summary>
    <div class="ligne"><label for="offset">Décalage (mm)</label><input type="number" id="offset" step="0.01" data-k="offset"></div>
    <div class="ligne"><label for="pasn">Pas d'une note (mm)</label><input type="number" id="pasn" step="0.001" data-k="pas"></div>
    <div class="ligne"><label for="nbleds">Nombre de LED</label><input type="number" id="nbleds" min="1" max="300" data-k="nbleds"></div>
    <div class="ligne"><label for="lpn">LED par note</label><select id="lpn" data-k="ledsparnote"><option>1</option><option>2</option></select></div>
    <div class="ligne"><label for="inverse">Sens du bandeau</label><select id="inverse" data-k="inverse"><option value="0">normal</option><option value="1">inversé</option></select></div>
    <div class="ligne"><label for="lum">Luminosité <b id="tlum" class="chiffres"></b></label><input id="lum" type="range" min="1" max="255" data-k="lum"></div>
    <div class="ligne"><label>Touche la plus à gauche : <b id="tbasse"></b></label><button id="bbasse">Capturer</button></div>
    <div class="ligne"><button id="bapercu">Aperçu</button><button id="beteindre">Éteindre l'aperçu</button></div>
    <p class="doux" id="wifi"></p>
  </details>
</section>
</main>
<div id="toast"></div>

<script>
const $=id=>document.getElementById(id);
const NOMS=['do','do#','ré','ré#','mi','fa','fa#','sol','sol#','la','la#','si'];
const nom=n=>NOMS[n%12]+(Math.floor(n/12)-1);
const temps=ms=>{const s=Math.floor(ms/1000);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0')};
let ws=null,e={},morceau=null,regl={},glisse=false,toastT=0;

function envoyer(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))}
function toast(t,ok=true){const d=$('toast');d.textContent=t;d.className='vu'+(ok?'':' err');clearTimeout(toastT);toastT=setTimeout(()=>d.className='',3500)}

function connecter(){
  ws=new WebSocket(`ws://${location.host}/ws`);
  ws.onopen=()=>{$('lien').className='ok';listerFichiers()};
  ws.onclose=()=>{$('lien').className='';setTimeout(connecter,2000)};
  ws.onmessage=ev=>{const m=JSON.parse(ev.data);
    if(m.t==='etat')majEtat(m);else if(m.t==='morceau')majMorceau(m);else if(m.t==='reglages')majReglages(m);
    else if(m.t==='fichiers')listerFichiers();else if(m.t==='msg')toast(m.texte,m.ok)};
}

const ETATS={vide:'',arret:'arrêté',lecture:'lecture',pause:'en pause',attente:'à toi de jouer…'};
function majEtat(m){
  const ancien=e.f;e=m;
  $('titre').textContent=m.f||'Aucun morceau chargé';
  if(ancien!==m.f)listerFichiersMarquer();
  $('etat').textContent=ETATS[m.e]||'';
  const vide=m.e==='vide';
  ['blecture','bpause','bstop','ba','bb','bab'].forEach(i=>$(i).disabled=vide);
  $('pos').max=Math.max(1,m.dur);
  if(!glisse){$('pos').value=m.pos;$('tpos').textContent=temps(m.pos)}
  $('tdur').textContent=temps(m.dur);
  if(document.activeElement!==$('vitesse')){$('vitesse').value=m.v;$('tvitesse').textContent=m.v+' %'}
  $('bpm').textContent=m.bpm?'('+m.bpm+' BPM)':'';
  document.querySelectorAll('#mode button').forEach(b=>b.classList.toggle('on',b.dataset.v===m.m));
  document.querySelectorAll('#travail button').forEach(b=>b.classList.toggle('on',+b.dataset.v===m.tr));
  $('travail').classList.toggle('cache',m.m!=='attente');
  $('stats').classList.toggle('cache',m.m!=='attente');
  $('boucle').checked=m.bo;
  $('tab').textContent=(m.a!==undefined?'A '+temps(m.a):'')+(m.b!==undefined?' → B '+temps(m.b):'');
  $('justes').querySelector('b').textContent=m.ok;$('erreurs').querySelector('b').textContent=m.err;
  const att=new Set(m.req||[]);
  $('notes').textContent=(m.notes||[]).map(n=>att.has(n)?'['+nom(n)+']':nom(n)).join(' ');
}

function majMorceau(m){
  morceau=m;$('smains').classList.remove('cache');
  const sel=$('partage');sel.value=m.partage;
  const g=$('groupes');g.innerHTML='';
  m.groupes.forEach(x=>{
    const d=document.createElement('div');d.className='groupe';
    d.innerHTML=`<div><b></b><br><span class="doux">${x.n} notes, ${nom(x.min)} à ${nom(x.max)}, canal ${x.canal}</span></div>`;
    d.querySelector('b').textContent=x.nom;
    const s=document.createElement('select');
    [['1','main droite'],['2','main gauche'],['3','par hauteur'],['0','masquée']].forEach(([v,t])=>s.add(new Option(t,v)));
    s.value=x.main;s.onchange=()=>envoyer({c:'main',g:x.id,v:+s.value});
    d.appendChild(s);g.appendChild(d);
  });
}

function majReglages(r){
  regl=r;
  const f=(id,v)=>{if(document.activeElement!==$(id))$(id).value=v};
  f('offset',r.offset);f('pasn',r.pas);f('nbleds',r.nbleds);f('lpn',r.lpn);f('inverse',r.inverse);f('lum',r.lum);
  f('antic',r.antic);f('horsplage',r.horsplage);f('cmd',r.cmd);f('cmg',r.cmg);
  $('mout').checked=!!r.mout;$('tlum').textContent=r.lum;$('tantic').textContent=r.antic?r.antic+' ms':'aucune';
  $('tbasse').textContent=nom(r.basse)+' (MIDI '+r.basse+')';
  $('toct').textContent='Clavier : '+nom(r.basse)+' à '+nom(r.basse+48);$('boctm').disabled=r.basse<12;$('boctp').disabled=r.basse>67;
  $('wifi').textContent='Wi-Fi : '+r.wifi;
}

async function listerFichiers(){
  try{
    const r=await fetch('/api/fichiers');const j=await r.json();const l=$('fichiers');l.innerHTML='';
    if(!j.fichiers.length)l.innerHTML='<p class="doux">Aucun fichier. Envoie un .mid ci-dessus.</p>';
    j.fichiers.sort((a,b)=>a.n.localeCompare(b.n)).forEach(x=>{
      const d=document.createElement('div');d.className='f';d.dataset.n=x.n;
      const s=document.createElement('span');s.textContent=x.n;d.appendChild(s);
      const t=document.createElement('small');t.className='doux';t.textContent=Math.ceil(x.o/1024)+' Ko';d.appendChild(t);
      const c=document.createElement('button');c.textContent='Charger';c.onclick=()=>envoyer({c:'charger',f:x.n});d.appendChild(c);
      const del=document.createElement('button');del.textContent='✕';del.title='Supprimer';
      del.onclick=async()=>{if(!confirm('Supprimer « '+x.n+' » ?'))return;
        const r=await fetch('/api/supprimer?f='+encodeURIComponent(x.n),{method:'POST'});toast(await r.text(),r.ok)};
      d.appendChild(del);l.appendChild(d);
    });
    $('place').textContent=Math.round(j.libre/1024)+' Ko libres sur '+Math.round(j.total/1024)+' Ko';
    listerFichiersMarquer();
  }catch(err){toast('Liste des fichiers indisponible',false)}
}
function listerFichiersMarquer(){document.querySelectorAll('#fichiers .f').forEach(d=>d.classList.toggle('charge',d.dataset.n===e.f))}

$('benvoi').onclick=async()=>{
  const fs=$('choix').files;if(!fs.length){toast('Choisis un fichier .mid',false);return}
  for(const f of fs){
    toast('Envoi de « '+f.name+' »…');
    const fd=new FormData();fd.append('fichier',f,f.name);
    try{const r=await fetch('/api/envoi',{method:'POST',body:fd});toast(r.ok?'« '+f.name+' » envoyé':await r.text(),r.ok)}
    catch(err){toast('Échec de l\'envoi',false)}
  }
  $('choix').value='';listerFichiers();
};

$('blecture').onclick=()=>envoyer({c:'lecture'});
$('bpause').onclick=()=>envoyer({c:'pause'});
$('bstop').onclick=()=>envoyer({c:'stop'});
$('pos').oninput=()=>{glisse=true;$('tpos').textContent=temps(+$('pos').value)};
$('pos').onchange=()=>{glisse=false;envoyer({c:'aller',v:+$('pos').value})};
$('vitesse').oninput=()=>{$('tvitesse').textContent=$('vitesse').value+' %';envoyer({c:'vitesse',v:+$('vitesse').value})};
document.querySelectorAll('#mode button').forEach(b=>b.onclick=()=>envoyer({c:'mode',v:b.dataset.v}));
document.querySelectorAll('#travail button').forEach(b=>b.onclick=()=>envoyer({c:'travail',v:+b.dataset.v}));
$('ba').onclick=()=>envoyer({c:'a'});$('bb').onclick=()=>envoyer({c:'b'});
$('bab').onclick=()=>envoyer({c:'ab_effacer'});
$('boucle').onchange=()=>envoyer({c:'boucle',v:$('boucle').checked});
$('partage').onchange=()=>envoyer({c:'partage',v:+$('partage').value});
$('cmd').onchange=()=>envoyer({c:'couleur',main:'md',v:$('cmd').value});
$('cmg').onchange=()=>envoyer({c:'couleur',main:'mg',v:$('cmg').value});
$('antic').oninput=()=>$('tantic').textContent=+$('antic').value?$('antic').value+' ms':'aucune';
$('antic').onchange=()=>envoyer({c:'reglage',k:'antic',v:+$('antic').value});
$('mout').onchange=()=>envoyer({c:'reglage',k:'mout',v:$('mout').checked?1:0});
$('horsplage').onchange=()=>envoyer({c:'reglage',k:'horsplage',v:+$('horsplage').value});
$('lum').oninput=()=>$('tlum').textContent=$('lum').value;
document.querySelectorAll('[data-k]').forEach(el=>el.onchange=()=>envoyer({c:'reglage',k:el.dataset.k,v:+el.value}));
$('bbasse').onclick=()=>envoyer({c:'basse'});
$('boctm').onclick=()=>envoyer({c:'octave',v:-1});$('boctp').onclick=()=>envoyer({c:'octave',v:1});
$('bapercu').onclick=()=>envoyer({c:'apercu'});
$('beteindre').onclick=()=>envoyer({c:'eteindre'});
for(let n=36;n<=96;n++)$('partage').add(new Option(nom(n)+' ('+n+')',n));
connecter();
</script>
</body>
</html>
)PAGE";
