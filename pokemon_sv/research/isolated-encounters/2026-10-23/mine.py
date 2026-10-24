# Research data processing, not an application test harness.
import ast,csv,re,json,math,hashlib,unicodedata,argparse
from pathlib import Path
from collections import defaultdict
parser=argparse.ArgumentParser(description='Research spawn-table proxies; not an application test harness.')
parser.add_argument('input_dir',type=Path)
parser.add_argument('output_dir',type=Path)
args=parser.parse_args()
D=args.input_dir;O=args.output_dir;O.mkdir(parents=True,exist_ok=True)
def readjs(name):
 s=(D/name).read_text();return ast.literal_eval(s[s.index('{'):s.index('};')+1])
species=list(csv.DictReader((D/'pokemon_species.csv').open()))
def norm(s):return unicodedata.normalize('NFKD',s).encode('ascii','ignore').decode().lower().replace(' ', '-').replace("'",'').replace('.','')
lookup={norm(x['identifier']):x for x in species}
alias={'sawsbuck-autum':'sawsbuck','sawsbuck-winter':'sawsbuck','flabb':'flabebe','tauros-paldean':'tauros','wooper-paldean':'wooper','flabebe':'flabebe'}
def family(name):
 key=alias.get(norm(name),norm(name));p=lookup.get(key)
 if p is None:raise ValueError('Unmapped species '+name)
 return p['evolution_chain_id']
chain_names=defaultdict(list)
for x in species:chain_names[x['evolution_chain_id']].append(x['identifier'])
roots={x['evolution_chain_id']:x['identifier'] for x in species if not x['evolves_from_species_id']}
# Parse the JS markers without executing third-party code.
s=(D/'pmarkers.js').read_text();s=re.sub(r"('icon'\s*:\s*)(\w+)",r"\1'\2'",s);markers=ast.literal_eval(s[s.index('{'):s.index('};')+1])
html=(D/'serebii-map.html').read_text(errors='replace')
flies=[]
for match in re.finditer(r'coords:\s*\[([^]]+)\],\s*icon:\s*(pcIcon|flyIcon),\s*tableID:\s*3001,\s*layer:\s*layFly',html):
 xyz=[float(x) for x in match.group(1).split(',')];flies.append({'id':'F%02d'%(len(flies)+1),'x':xyz[0],'z':xyz[2],'kind':match.group(2)})
assert len(flies)>40
(O/'flypoints.json').write_text(json.dumps(flies,indent=2))
rows=[];summary={};coverage=[];unknown=[]
for version in ['scarlet','violet']:
 calc=readjs('pokemon_calculated_'+version+'.js');areas=readjs('area_type_pokemon_'+version+'.js')
 for name,observations in calc.items():
  for observation in observations:
   for table,types in observation.items():
    marker=markers.get(table)
    if not marker:raise ValueError("Missing marker for table "+str(table))
    for typ,pct in types:
     x,y,z=marker['coords'];fly=min(flies,key=lambda f:math.hypot(x-f['x'],z-f['z']));dist=math.hypot(x-fly['x'],z-fly['z'])
     rows.append({'version':version,'species':name,'family':roots[family(name)],'type':typ,'optimizer_score':pct,'table_id':table,'x':x,'z':z,'nearest_flypoint':fly['id'],'fly_x':fly['x'],'fly_z':fly['z'],'center_distance_map_units':round(dist,1),'center_distance_grid_cells':round(dist/250,2),'distance_low_map_units':round(max(0,dist-math.sqrt(2)*125),1),'distance_high_map_units':round(dist+math.sqrt(2)*125,1)})
     rows[-1]['location_quality']='coarse_grid_center'
     if table in (828,829,830):
      rows[-1]['location_quality']='area_zero_placeholder'
      for k in ['nearest_flypoint','fly_x','fly_z','center_distance_map_units','center_distance_grid_cells','distance_low_map_units','distance_high_map_units']:rows[-1][k]=None
 types=sorted(set(t for a in areas.values() for t in a))
 for typ in types:
  strict=[r for r in rows if r['version']==version and r['type']==typ and r['optimizer_score']==100]
  family_exclusive=set();family_species=set();exclusive_tables=0
  for table,pools in areas.items():
   pool=pools.get(typ,[])
   fams={family(p[0]) for p in pool}
   if len(fams)==1:
    family_exclusive.update(fams);family_species.update(p[0] for p in pool);exclusive_tables+=1
  coverage.append({'version':version,'type':typ,'strict_species':len({r['species'] for r in strict}),'strict_families':len({r['family'] for r in strict}),'family_only_families':len(family_exclusive),'family_only_species':len(family_species),'strict_table_type_pairs':len({r['table_id'] for r in strict}),'family_only_table_type_pairs':exclusive_tables,'strict_family_names':'; '.join(sorted({r['family'] for r in strict}))})
 active={n for n,v in calc.items() if v};strictspecies={r['species'] for r in rows if r['version']==version and r['optimizer_score']==100};strictfams={r['family'] for r in rows if r['version']==version and r['optimizer_score']==100}
 familyfams=set()
 for pool in areas.values():
  for group in pool.values():
   fs={roots[family(p[0])] for p in group}
   if len(fs)==1:familyfams.update(fs)
 summary[version]={'species_form_labels_with_any_record':len(active),'species_with_any_record':len({lookup[alias.get(norm(n),norm(n))]['id'] for n in active}),'families_with_any_record':len({family(n) for n in active}),'species_form_labels_with_strict_100':len(strictspecies),'species_with_strict_100':len({lookup[alias.get(norm(n),norm(n))]['id'] for n in strictspecies}),'families_with_strict_100':len(strictfams),'families_with_family_exclusive_pool':len(familyfams),'spawn_tables':len(areas),'strict100_family_list':sorted(strictfams)}
for filename,records in [('candidate_cells.csv',rows),('coverage_by_type.csv',coverage)]:
 with (O/filename).open('w') as f:
  w=csv.DictWriter(f,fieldnames=list(records[0]));w.writeheader();w.writerows(records)
# Greedy coverage of strict table proxies, with nearby access; no terrain claim.
candidates=[]
for version in ['scarlet','violet']:
 cells=defaultdict(list)
 for r in rows:
  if r['version']==version and r['optimizer_score']==100:cells[r['table_id']].append(r)
 covered=set()
 for rank in range(1,9):
  choices=[(len({r['family'] for r in rs}-covered),-rs[0]['center_distance_map_units'],tid,rs) for tid,rs in cells.items() if rs[0]['center_distance_grid_cells'] is not None and rs[0]['center_distance_grid_cells']<=1]
  gain,_,tid,rs=max(choices,key=lambda x:(x[0],x[1]))
  if not gain:break
  fs={r['family'] for r in rs};covered.update(fs)
  candidates.append({'version':version,'selection_rank':rank,'table_id':tid,'new_families':gain,'cumulative_families':len(covered),'families':'; '.join(sorted(fs)),'species_and_types':'; '.join(sorted(r['species']+':'+r['type'] for r in rs)),'x':rs[0]['x'],'z':rs[0]['z'],'nearest_flypoint':rs[0]['nearest_flypoint'],'center_distance_grid_cells':rs[0]['center_distance_grid_cells'],'terrain':'unknown','live_verified':False})
  cells.pop(tid)
with (O/'data_first_shortlist.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(candidates[0]));w.writeheader();w.writerows(candidates)
summary['flypoints']=len(flies);summary['candidate_rows']=len(rows)
(O/'summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps({k:{x:y for x,y in v.items() if x!='strict100_family_list'} if isinstance(v,dict) else v for k,v in summary.items()},indent=2))
for v in ['scarlet','violet']:
 print(v)
 for r in coverage:
  if r['version']==v:print(r['type'],r['strict_species'],r['strict_families'],r['family_only_families'])

# Cross-check extraction against the optimizer author's separate strict table.
validation={}
for version in ['scarlet','violet']:
 author={(table,typ,name) for typ,parts in readjs('type_success_'+version+'.js').items() for part in parts for table,entries in part.items() for name,pct in entries if pct==100}
 extracted={(r['table_id'],r['type'],r['species']) for r in rows if r['version']==version and r['optimizer_score']==100}
 if author!=extracted:raise ValueError('Author strict table mismatch for '+version)
 validation[version]={'strict_records':len(extracted),'author_records':len(author),'differences':len(author^extracted)}
summary['cross_check']=validation
(O/'summary.json').write_text(json.dumps(summary,indent=2))
with (O/'strict_candidates.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(r for r in rows if r['optimizer_score']==100)
