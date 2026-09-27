#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, math, struct
from collections import Counter, defaultdict
from pathlib import Path

HEADER=struct.Struct('<8sIIIIq'+'I'*14)
COMMON=struct.Struct('<qqIIIIII')
RECORD_SIZE=512
PAYLOAD=40
TYPES={1:'module',2:'di8_create',3:'create_device',4:'set_data_format',5:'set_coop',6:'acquire',7:'unacquire',8:'get_state',9:'get_data_call',10:'get_data_axis',11:'d820',12:'raw_reg',13:'stats',14:'meta',15:'iat',16:'di8_vtable',17:'mouse_vtable',18:'create_device_enter',19:'ab9830'}

def cstr(b): return b.split(b'\0',1)[0].decode('utf-8','replace')
def caller_name(mods,base,rva): return f"{mods.get(base,f'0x{base:08X}')}+0x{rva:08X}"
def write_csv(path,rows):
    rows=list(rows)
    if not rows:return
    keys=[];seen=set()
    for row in rows:
        for k in row:
            if k not in seen:seen.add(k);keys.append(k)
    with open(path,'w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('path',type=Path);ns=ap.parse_args();raw=ns.path.read_bytes()
    if len(raw)<HEADER.size:raise SystemExit('file too small')
    vals=HEADER.unpack_from(raw,0);magic,version,header_size,record_size,flags,qpc,*rest=vals
    timestamp,image_size,text_rva,queue_cap,flush_ms,module_ms,rawreg_ms,*_=rest
    if magic!=b'DS3LM82\0' or version!=82 or header_size!=HEADER.size or record_size!=RECORD_SIZE:raise SystemExit(f'unsupported header magic={magic!r} version={version}')
    body=raw[header_size:];trailing=len(body)%record_size;nrec=len(body)//record_size
    records=[];counts=Counter()
    for i in range(nrec):
        off=header_size+i*record_size;c=COMMON.unpack_from(raw,off);r={'qpc_begin':c[0],'qpc_end':c[1],'sequence':c[2],'type':c[3],'thread':c[4],'caller_address':c[5],'caller_module_base':c[6],'caller_rva':c[7],'off':off};records.append(r);counts[r['type']]+=1
    mods={};module_rows=[]
    for r in records:
        if r['type']!=1:continue
        p=r['off']+PAYLOAD;base,size=struct.unpack_from('<II',raw,p);name=cstr(raw[p+8:p+136]);path=cstr(raw[p+136:p+436]);mods[base]=name;module_rows.append({'base':f'0x{base:08X}','size':f'0x{size:08X}','name':name,'path':path})
    print('DS3 Linear Mouse Fix - Experimental A/B v0.8.2')
    print(f'qpc_frequency={qpc} timestamp=0x{timestamp:08X} image_size=0x{image_size:08X}')
    print(f'records={nrec} trailing_bytes={trailing} types='+', '.join(f'{TYPES.get(k,k)}:{v}' for k,v in sorted(counts.items())))
    state_rows=[];ab_rows=[];d820_rows=[];latest_stats=None
    state_summary=defaultdict(lambda:{'records':0,'nonzero':0,'sum_x':0,'sum_y':0,'max_abs_x':0,'max_abs_y':0})
    for r in records:
        p=r['off']+PAYLOAD;typ=r['type'];caller=caller_name(mods,r['caller_module_base'],r['caller_rva']);t=r['qpc_begin']/qpc if qpc else 0
        if typ==8:
            u=struct.unpack_from('<11I3i',raw,p);row={'time_s':t,'qpc_begin':r['qpc_begin'],'qpc_end':r['qpc_end'],'caller':caller,'caller_rva':r['caller_rva'],'thread':r['thread'],'valid_xyz':u[10],'x':u[11],'y':u[12],'z':u[13]};state_rows.append(row);s=state_summary[caller];s['records']+=1
            if u[10]:s['sum_x']+=u[11];s['sum_y']+=u[12];s['max_abs_x']=max(s['max_abs_x'],abs(u[11]));s['max_abs_y']=max(s['max_abs_y'],abs(u[12]));s['nonzero']+=int(u[11]!=0 or u[12]!=0)
        elif typ==19:
            u=struct.unpack_from('<8I8f2i2I',raw,p);fl=u[7]
            ab_rows.append({'time_s':t,'qpc_begin':r['qpc_begin'],'qpc_end':r['qpc_end'],'caller':caller,'thread':r['thread'],'pre_x':u[10],'pre_y':u[11],'pre_magnitude':math.hypot(u[10],u[11]),'vanilla_post_x':u[12],'vanilla_post_y':u[13],'effective_post_x':u[14],'effective_post_y':u[15],'fresh_di_x':u[16],'fresh_di_y':u[17],'bypass_applied':int(bool(fl&16)),'profile':'HIP' if u[4]==0x0033F3ED else ('ADS' if u[4]==0x00149F17 else 'OTHER')})
        elif typ==11:
            head=struct.unpack_from('<4I5f',raw,p);selfp,obj0,obj4,dtbits,dt,ix,iy,ox,oy=head
            result=struct.unpack_from('<i',raw,p+132)[0]
            vanilla_x,vanilla_y,pre_x,pre_y,scale=struct.unpack_from('<5f',raw,p+136)
            cflags,tail_gen,tail_age=struct.unpack_from('<3I',raw,p+156)
            d820_rows.append({'time_s':t,'qpc_begin':r['qpc_begin'],'qpc_end':r['qpc_end'],'caller':caller,'caller_rva':r['caller_rva'],'thread':r['thread'],'self':f'0x{selfp:08X}','dt':dt,'in_x':ix,'in_y':iy,'vanilla_out_x':vanilla_x,'vanilla_out_y':vanilla_y,'corrected_out_x':ox,'corrected_out_y':oy,'model_preclamp_x':pre_x,'model_preclamp_y':pre_y,'model_scale':scale,'model_valid':int(bool(cflags&1)),'target_camera':int(bool(cflags&2)),'tail_latch_active':int(bool(cflags&4)),'preclamp_over_limit':int(bool(cflags&8)),'unclamp_applied':int(bool(cflags&16)),'tail_correction':int(bool(cflags&32)),'tail_generation':tail_gen,'tail_age_us':tail_age*1_000_000.0/qpc if qpc else 0,'result':result,'profile':'HIP' if r['caller_rva']==0x0033F407 else ('ADS' if r['caller_rva']==0x00149F2E else 'OTHER')})
        elif typ==13:
            latest_stats=struct.unpack_from('<48I',raw,p)
    print('\nDirectInput GetDeviceState:')
    for c,s in state_summary.items():print(f"  caller={c} records={s['records']} nonzero={s['nonzero']} sumX={s['sum_x']} sumY={s['sum_y']} maxAbsX={s['max_abs_x']} maxAbsY={s['max_abs_y']}")
    print('\nAB9830:')
    print(f"  targeted={len(ab_rows)} bypass_applied={sum(x['bypass_applied'] for x in ab_rows)}")
    print('\nD820 experimental correction:')
    print(f"  records={len(d820_rows)} model_valid={sum(x['model_valid'] for x in d820_rows)} target_camera={sum(x['target_camera'] for x in d820_rows)} unclamp_applied={sum(x['unclamp_applied'] for x in d820_rows)} tail_corrections={sum(x['tail_correction'] for x in d820_rows)}")
    changed=[x for x in d820_rows if x['unclamp_applied']]
    if changed:
        print(f"  max_abs_vanilla_to_corrected={max(max(abs(x['corrected_out_x']-x['vanilla_out_x']),abs(x['corrected_out_y']-x['vanilla_out_y'])) for x in changed):.9g}")
    if latest_stats:
        names=['enqueued','dropped','dropped_di','dropped_d820','highwater','write_errors','di8create','create_device','mouse_devices','setformat','setcoop','acquire','unacquire','getstate','getstate_packets','getstate_nonzero','getdata','getdata_axis','getdata_nonzero','d820','modules','rawreg_polls','rawreg_changes','rawreg_entries','iat_patched','iat_conflict','dropped_curve','abc510_target_calls','ab9830_calls','ab9830_targeted','ab9830_hook_installed','linear_fix_enabled','ab9830_bypass_applied','fresh_mouse_reads','fresh_mouse_claims','fresh_mouse_nonzero_claims','fresh_mouse_stale_rejects','d820_unclamp_applied','d820_tail_applied','d820_model_failures','tail_starts','tail_timeouts','tail_stable_exits','hip_ab_bypass','hip_d820_unclamp','ads_ab_bypass','ads_d820_unclamp','ads_tail']
        print('\nLatest logger stats: '+' '.join(f'{k}={v}' for k,v in zip(names,latest_stats)))
        print('CAPTURE_INTEGRITY='+('OK' if latest_stats[1]==0 and latest_stats[5]==0 and trailing==0 else 'CHECK'))
    prefix=ns.path.with_suffix('');write_csv(str(prefix)+'_get_state.csv',state_rows);write_csv(str(prefix)+'_ab9830.csv',ab_rows);write_csv(str(prefix)+'_d820.csv',d820_rows);write_csv(str(prefix)+'_modules.csv',module_rows)
    print(f'\nCSV prefix: {prefix}_*.csv')
if __name__=='__main__':main()
