#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, math, struct
from collections import Counter, defaultdict
from pathlib import Path

HEADER = struct.Struct('<8sIIIIq' + 'I'*14)
COMMON = struct.Struct('<qqIIIIII')
RECORD_SIZE = 512
PAYLOAD = 40
TYPES = {
    1:'module',2:'di8_create',3:'create_device',4:'set_data_format',5:'set_coop',6:'acquire',7:'unacquire',
    8:'get_state',9:'get_data_call',10:'get_data_axis',11:'d820',12:'raw_reg',13:'stats',14:'meta',15:'iat',
    16:'di8_vtable',17:'mouse_vtable',18:'create_device_enter',19:'ab9830'
}

def cstr(b: bytes) -> str:
    return b.split(b'\0',1)[0].decode('utf-8','replace')

def guid_text(raw: bytes) -> str:
    d1,d2,d3,*d4 = struct.unpack('<IHH8B', raw)
    return f'{d1:08X}-{d2:04X}-{d3:04X}-' + ''.join(f'{x:02X}' for x in d4[:2]) + '-' + ''.join(f'{x:02X}' for x in d4[2:])

def f32_from_bits(v: int) -> float:
    return struct.unpack('<f', struct.pack('<I',v))[0]

def caller_name(mods, base, rva):
    return f"{mods.get(base, f'0x{base:08X}')}+0x{rva:08X}"

def write_csv(path, rows):
    rows=list(rows)
    if not rows: return
    keys=[]
    seen=set()
    for row in rows:
        for k in row:
            if k not in seen: seen.add(k); keys.append(k)
    with open(path,'w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=keys); w.writeheader(); w.writerows(rows)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('path',type=Path)
    ns=ap.parse_args()
    raw=ns.path.read_bytes()
    if len(raw)<HEADER.size: raise SystemExit('file too small')
    vals=HEADER.unpack_from(raw,0)
    magic,version,header_size,record_size,flags,qpc,*rest=vals
    timestamp,image_size,text_rva,queue_cap,flush_ms,module_ms,rawreg_ms,*_reserved=rest
    if magic!=b'DS3DI70\0' or version!=70 or header_size!=HEADER.size or record_size!=RECORD_SIZE:
        raise SystemExit(f'unsupported header magic={magic!r} version={version} header={header_size} record={record_size}')
    body=raw[header_size:]
    trailing=len(body)%record_size
    nrec=len(body)//record_size
    records=[]; counts=Counter()
    for i in range(nrec):
        off=header_size+i*record_size
        c=COMMON.unpack_from(raw,off)
        rec={'qpc_begin':c[0],'qpc_end':c[1],'sequence':c[2],'type':c[3],'thread':c[4],'caller_address':c[5],'caller_module_base':c[6],'caller_rva':c[7],'off':off}
        counts[rec['type']]+=1; records.append(rec)
    mods={}
    mod_sizes={}
    module_rows=[]
    for r in records:
        if r['type']!=1: continue
        p=r['off']+PAYLOAD
        base,size=struct.unpack_from('<II',raw,p)
        name=cstr(raw[p+8:p+136]); path=cstr(raw[p+136:p+436])
        mods[base]=name; mod_sizes[base]=size
        module_rows.append({'base':f'0x{base:08X}','size':f'0x{size:08X}','name':name,'path':path})
    print('DS3 DirectInput + D820 + AB9830 Probe v0.7')
    print(f'qpc_frequency={qpc} timestamp=0x{timestamp:08X} image_size=0x{image_size:08X}')
    print(f'records={nrec} trailing_bytes={trailing} types=' + ', '.join(f'{TYPES.get(k,k)}:{v}' for k,v in sorted(counts.items())))
    print(f'modules_mapped={len(mods)}')

    di8_rows=[]; create_enter_rows=[]; create_rows=[]; fmt_rows=[]; coop_rows=[]; acq_rows=[]; state_rows=[]; data_call_rows=[]; axis_rows=[]; d820_rows=[]; ab9830_rows=[]; rawreg_rows=[]; iat_rows=[]; vtable_rows=[]
    state_summary=defaultdict(lambda:{'records':0,'valid':0,'nonzero':0,'sum_x':0,'sum_y':0,'max_abs_x':0,'max_abs_y':0})
    axis_summary=defaultdict(lambda:{'events':0,'nonzero':0,'sum_x':0,'sum_y':0,'max_abs':0})
    d820_summary=defaultdict(lambda:{'records':0,'max_abs_in':0.0,'max_abs_out':0.0})
    latest_stats=None
    for r in records:
        p=r['off']+PAYLOAD; typ=r['type']; caller=caller_name(mods,r['caller_module_base'],r['caller_rva'])
        t=(r['qpc_begin']/qpc if qpc else 0)
        if typ==2:
            u=struct.unpack_from('<10I',raw,p); guid=guid_text(raw[p+40:p+56])
            row={'time_s':f'{t:.9f}','caller':caller,'thread':r['thread'],'version':f'0x{u[2]:08X}','iid':guid,'hr':f'0x{u[6]:08X}','last_error':u[7],'object':f'0x{u[8]:08X}','slot3_patched':u[9]}; di8_rows.append(row)
        elif typ==3:
            u=struct.unpack_from('<10I',raw,p); guid=guid_text(raw[p+40:p+56])
            create_rows.append({'time_s':f'{t:.9f}','caller':caller,'thread':r['thread'],'guid':guid,'hr':f'0x{u[5]:08X}','last_error':u[6],'device':f'0x{u[7]:08X}','is_mouse_guid':u[8],'vtable_patched':u[9]})
        elif typ==18:
            u=struct.unpack_from('<10I',raw,p); guid=guid_text(raw[p+40:p+56])
            create_enter_rows.append({'time_s':f'{t:.9f}','caller':caller,'thread':r['thread'],'call_id':u[0],'self':f'0x{u[1]:08X}','guid':guid,'is_mouse_guid':u[8]})
        elif typ in (16,17):
            head=struct.unpack_from('<7I',raw,p); slots=struct.unpack_from('<16I',raw,p+28)
            obj,vt,kind,slot_count,owner,patched,conflict=head
            row={'time_s':f'{t:.9f}','type':TYPES[typ],'object':f'0x{obj:08X}','vtable':f'0x{vt:08X}','interface_kind':kind,'slot_count':slot_count,'owner_module':mods.get(owner,f'0x{owner:08X}'),'patched_mask':f'0x{patched:08X}','conflict_mask':f'0x{conflict:08X}'}
            for i in range(min(slot_count,16)):
                addr=slots[i]; base=0; rva=0
                for mb,size in mod_sizes.items():
                    if mb<=addr<mb+size: base=mb; rva=addr-mb; break
                if base: row[f'slot{i}']=f'{mods.get(base,hex(base))}+0x{rva:08X}'
                else: row[f'slot{i}']=f'0x{addr:08X}'
            vtable_rows.append(row)
        elif typ==4:
            u=struct.unpack_from('<16I',raw,p)
            fmt_rows.append({'time_s':f'{t:.9f}','caller':caller,'device':f'0x{u[0]:08X}','data_size':u[5],'num_objs':u[6],'readable':u[7],'x_offset':f'0x{u[8]:08X}','y_offset':f'0x{u[9]:08X}','z_offset':f'0x{u[10]:08X}','format_kind':u[11],'captured_objects':u[12],'hr':f'0x{u[13]:08X}','last_error':u[14]})
        elif typ in (5,6,7):
            u=struct.unpack_from('<5I',raw,p)
            acq_rows.append({'time_s':f'{t:.9f}','type':TYPES[typ],'caller':caller,'device':f'0x{u[0]:08X}','arg1':f'0x{u[1]:08X}','arg2':f'0x{u[2]:08X}','hr':f'0x{u[3] if typ==5 else u[1]:08X}'})
        elif typ==8:
            u=struct.unpack_from('<11I3i',raw,p)
            row={'time_s':f'{t:.9f}','qpc_begin':r['qpc_begin'],'qpc_end':r['qpc_end'],'caller':caller,'caller_rva':r['caller_rva'],'thread':r['thread'],'device':f'0x{u[0]:08X}','cb_data':u[1],'hr':f'0x{u[3]:08X}','last_error':u[4],'format_kind':u[5],'x_offset':f'0x{u[6]:08X}','y_offset':f'0x{u[7]:08X}','z_offset':f'0x{u[8]:08X}','buffer_readable':u[9],'valid_xyz':u[10],'x':u[11],'y':u[12],'z':u[13]}; state_rows.append(row)
            s=state_summary[caller]; s['records']+=1
            if u[10]: s['valid']+=1; s['sum_x']+=u[11]; s['sum_y']+=u[12]; s['max_abs_x']=max(s['max_abs_x'],abs(u[11])); s['max_abs_y']=max(s['max_abs_y'],abs(u[12])); s['nonzero']+=int(u[11]!=0 or u[12]!=0)
        elif typ==9:
            u=struct.unpack_from('<18I',raw,p)
            data_call_rows.append({'time_s':f'{t:.9f}','caller':caller,'thread':r['thread'],'call_id':u[0],'device':f'0x{u[1]:08X}','cb_object_data':u[2],'count_before':u[5],'count_after':u[6],'flags':f'0x{u[7]:08X}','hr':f'0x{u[8]:08X}','last_error':u[9],'format_kind':u[10],'parsed_objects':u[15],'axis_events':u[16],'truncated':u[17]})
        elif typ==10:
            u=struct.unpack_from('<9Ii',raw,p)
            row={'time_s':f'{t:.9f}','caller':caller,'thread':r['thread'],'call_id':u[0],'device':f'0x{u[1]:08X}','event_index':u[2],'object_offset':f'0x{u[3]:08X}','raw_data':f'0x{u[4]:08X}','device_timestamp':u[5],'device_sequence':u[6],'axis_kind':u[7],'hr':f'0x{u[8]:08X}','delta':u[9]}; axis_rows.append(row)
            s=axis_summary[caller]; s['events']+=1; s['max_abs']=max(s['max_abs'],abs(u[9])); s['nonzero']+=int(u[9]!=0)
            if u[7]==1:s['sum_x']+=u[9]
            elif u[7]==2:s['sum_y']+=u[9]
        elif typ==11:
            head=struct.unpack_from('<4I5f',raw,p)
            selfp,obj0,obj4,dtbits,dt,ix,iy,ox,oy=head
            result=struct.unpack_from('<i',raw,p+4*4+5*4+48+48)[0]
            row={'time_s':f'{t:.9f}','qpc_begin':r['qpc_begin'],'qpc_end':r['qpc_end'],'caller':caller,'caller_rva':r['caller_rva'],'thread':r['thread'],'self':f'0x{selfp:08X}','dt':dt,'in_x':ix,'in_y':iy,'out_x':ox,'out_y':oy,'result':result}; d820_rows.append(row)
            s=d820_summary[caller]; s['records']+=1; s['max_abs_in']=max(s['max_abs_in'],abs(ix),abs(iy)); s['max_abs_out']=max(s['max_abs_out'],abs(ox),abs(oy))
        elif typ==19:
            u=struct.unpack_from('<8I6f',raw,p)
            pre_mag=math.hypot(u[10],u[11]); post_mag=math.hypot(u[12],u[13])
            ab9830_rows.append({'time_s':f'{t:.9f}','qpc_begin':r['qpc_begin'],'qpc_end':r['qpc_end'],'caller':caller,'caller_rva':r['caller_rva'],'thread':r['thread'],
                'self':f'0x{u[0]:08X}','x_ptr':f'0x{u[1]:08X}','y_ptr':f'0x{u[2]:08X}','direct_return_rva':f'0x{u[3]:08X}','parent_return_rva':f'0x{u[4]:08X}',
                'target_d820_return_rva':f'0x{u[5]:08X}','path_gate_depth':u[6],'curve_arg0':u[8],'curve_arg1':u[9],
                'pre_x':u[10],'pre_y':u[11],'pre_magnitude':pre_mag,'post_x':u[12],'post_y':u[13],'post_magnitude':post_mag,
                'pre_over_1':int(pre_mag>1.0),'post_near_1':int(0.999<=post_mag<=1.001)})
        elif typ==12:
            u=struct.unpack_from('<10I',raw,p)
            rawreg_rows.append({'time_s':f'{t:.9f}','snapshot_id':u[0],'index':u[1],'total':u[2],'usage_page':u[3],'usage':u[4],'flags':f'0x{u[5]:08X}','hwnd':f'0x{u[6]:08X}','result':f'0x{u[7]:08X}','last_error':u[8],'changed':u[9]})
        elif typ==13:
            latest_stats=struct.unpack_from('<31I',raw,p)
        elif typ==15:
            u=struct.unpack_from('<6I',raw,p)
            iat_rows.append({'slot_rva':f'0x{u[0]:08X}','previous_target':f'0x{u[1]:08X}','previous_module':mods.get(u[2],f'0x{u[2]:08X}'),'previous_rva':f'0x{u[3]:08X}','patched':u[4],'conflict':u[5]})

    print('\nDirectInput8Create:')
    if not di8_rows: print('  calls=0')
    else:
        print(f'  calls={len(di8_rows)} slot3_patched={sum(int(x["slot3_patched"]) for x in di8_rows)}')
        for k,v in Counter(x['caller'] for x in di8_rows).items(): print(f'  caller={k} calls={v}')
    print('\nCreateDevice:')
    if not create_rows: print('  calls=0')
    else:
        print(f'  enters={len(create_enter_rows)} returns={len(create_rows)} mouse_guid={sum(int(x["is_mouse_guid"]) for x in create_rows)} mouse_vtable_patched={sum(int(x["vtable_patched"]) for x in create_rows)}')
        for x in create_rows: print(f"  caller={x['caller']} guid={x['guid']} mouse={x['is_mouse_guid']} vtable_patched={x['vtable_patched']} hr={x['hr']}")
    print('\nVtable diagnostics:')
    if not vtable_rows: print('  none')
    else:
        for x in vtable_rows:
            print(f"  {x['type']} object={x['object']} vtable={x['vtable']} owner={x['owner_module']} patched={x['patched_mask']} conflict={x['conflict_mask']}")
            if x['type']=='di8_vtable':
                print(f"    slot3 CreateDevice={x.get('slot3','?')} slot4 EnumDevices={x.get('slot4','?')}")
    print('\nMouse data format:')
    if not fmt_rows: print('  SetDataFormat calls=0')
    else:
        for x in fmt_rows: print(f"  caller={x['caller']} size={x['data_size']} objs={x['num_objs']} kind={x['format_kind']} x={x['x_offset']} y={x['y_offset']} hr={x['hr']}")
    print('\nGetDeviceState:')
    if not state_summary: print('  calls=0')
    for c,s in state_summary.items(): print(f"  caller={c} records={s['records']} valid={s['valid']} nonzero={s['nonzero']} sumX={s['sum_x']} sumY={s['sum_y']} maxAbsX={s['max_abs_x']} maxAbsY={s['max_abs_y']}")
    print('\nGetDeviceData axis events:')
    if not axis_summary: print('  events=0')
    for c,s in axis_summary.items(): print(f"  caller={c} events={s['events']} nonzero={s['nonzero']} sumX={s['sum_x']} sumY={s['sum_y']} maxAbs={s['max_abs']}")
    confirmed=any(s['nonzero'] for s in state_summary.values()) or any(s['nonzero'] for s in axis_summary.values())
    print(f'DIRECTINPUT_MOUSE_CONFIRMED={"YES" if confirmed else "NO"}')
    print('\nAB9830 targeted camera path:')
    print(f'  records={len(ab9830_rows)}')
    if ab9830_rows:
        pre_over=sum(int(x['pre_over_1']) for x in ab9830_rows); post_unit=sum(int(x['post_near_1']) for x in ab9830_rows)
        print(f'  pre_magnitude>1={pre_over} post_magnitude~1={post_unit}')
        print(f"  max_pre_magnitude={max(float(x['pre_magnitude']) for x in ab9830_rows):.9g} max_post_magnitude={max(float(x['post_magnitude']) for x in ab9830_rows):.9g}")
        for c,v in Counter(x['caller'] for x in ab9830_rows).items(): print(f'  caller={c} records={v}')
    print('\nPassive Raw Input registration snapshots:')
    if not rawreg_rows: print('  none recorded')
    else:
        latest=max(int(x['snapshot_id']) for x in rawreg_rows); rows=[x for x in rawreg_rows if int(x['snapshot_id'])==latest]
        print(f'  snapshots={len(set(x["snapshot_id"] for x in rawreg_rows))} latest_id={latest} latest_entries={rows[0]["total"] if rows else 0}')
        for x in rows: print(f"  usagePage={x['usage_page']} usage={x['usage']} flags={x['flags']} hwnd={x['hwnd']}")
    print('\nD820:')
    print(f'  records={len(d820_rows)}')
    for c,s in sorted(d820_summary.items(),key=lambda kv:-kv[1]['records']): print(f"  caller={c} records={s['records']} max_abs_in={s['max_abs_in']:.9g} max_abs_out={s['max_abs_out']:.9g}")
    if iat_rows:
        print('\nDirectInput IAT:')
        for x in iat_rows: print(f"  slot={x['slot_rva']} previous={x['previous_module']}+{x['previous_rva']} patched={x['patched']} conflict={x['conflict']}")
    if latest_stats:
        names=['enqueued','dropped','dropped_di','dropped_d820','highwater','write_errors','di8create','create_device','mouse_devices','setformat','setcoop','acquire','unacquire','getstate','getstate_packets','getstate_nonzero','getdata','getdata_axis','getdata_nonzero','d820','modules','rawreg_polls','rawreg_changes','rawreg_entries','iat_patched','iat_conflict','dropped_curve','abc510_target_calls','ab9830_calls','ab9830_targeted','ab9830_hook_installed']
        print('\nLatest logger stats: ' + ' '.join(f'{k}={v}' for k,v in zip(names,latest_stats)))
        integrity=(latest_stats[1]==0 and latest_stats[5]==0 and trailing==0)
        print(f'CAPTURE_INTEGRITY={"OK" if integrity else "CHECK"}')
    # Offline synchronization: nearest previous DirectInput state and nearest following D820 camera-path sample.
    pipeline_rows=[]
    if ab9830_rows:
        states_by_thread=defaultdict(list); d820_by_thread=defaultdict(list)
        for x in state_rows:
            if int(x['valid_xyz']): states_by_thread[int(x['thread'])].append(x)
        for x in d820_rows:
            if int(x['caller_rva'])==0x0033F407: d820_by_thread[int(x['thread'])].append(x)
        for v in states_by_thread.values(): v.sort(key=lambda z:int(z['qpc_end']))
        for v in d820_by_thread.values(): v.sort(key=lambda z:int(z['qpc_begin']))
        import bisect
        matched=0; diffs=[]; gaps_di=[]; gaps_d=[]
        for a in ab9830_rows:
            tid=int(a['thread']); qb=int(a['qpc_begin']); qe=int(a['qpc_end']); st=states_by_thread.get(tid,[]); db=d820_by_thread.get(tid,[])
            sr=None; dr=None
            if st:
                keys=[int(z['qpc_end']) for z in st]; i=bisect.bisect_right(keys,qb)-1
                if i>=0 and qb-keys[i] <= qpc*0.020: sr=st[i]
            if db:
                keys=[int(z['qpc_begin']) for z in db]; j=bisect.bisect_left(keys,qe)
                if j<len(db) and keys[j]-qe <= qpc*0.020: dr=db[j]
            row={'time_s':a['time_s'],'thread':tid,'di_x':sr['x'] if sr else '', 'di_y':sr['y'] if sr else '',
                 'di_to_ab_ms':((qb-int(sr['qpc_end']))*1000.0/qpc) if sr else '',
                 'pre_x':a['pre_x'],'pre_y':a['pre_y'],'pre_magnitude':a['pre_magnitude'],'post_x':a['post_x'],'post_y':a['post_y'],'post_magnitude':a['post_magnitude'],
                 'ab_to_d820_ms':((int(dr['qpc_begin'])-qe)*1000.0/qpc) if dr else '', 'd820_in_x':dr['in_x'] if dr else '', 'd820_in_y':dr['in_y'] if dr else '', 'd820_out_x':dr['out_x'] if dr else '', 'd820_out_y':dr['out_y'] if dr else ''}
            if sr: gaps_di.append(float(row['di_to_ab_ms']))
            if dr:
                gaps_d.append(float(row['ab_to_d820_ms'])); diffs.append(max(abs(float(a['post_x'])-float(dr['in_x'])),abs(float(a['post_y'])-float(dr['in_y'])))); matched+=1
            pipeline_rows.append(row)
        print('\nPipeline synchronization:')
        print(f'  AB9830->D820 matched={matched}/{len(ab9830_rows)}')
        if diffs: print(f'  max_abs(post_AB9830 - D820_input)={max(diffs):.9g}')
        if gaps_di: print(f'  median_DI_to_AB9830_ms={sorted(gaps_di)[len(gaps_di)//2]:.6g}')
        if gaps_d: print(f'  median_AB9830_to_D820_ms={sorted(gaps_d)[len(gaps_d)//2]:.6g}')

    prefix=ns.path.with_suffix('')
    write_csv(str(prefix)+'_modules.csv',module_rows); write_csv(str(prefix)+'_di8_create.csv',di8_rows); write_csv(str(prefix)+'_create_device_enter.csv',create_enter_rows); write_csv(str(prefix)+'_create_device.csv',create_rows); write_csv(str(prefix)+'_vtables.csv',vtable_rows)
    write_csv(str(prefix)+'_set_data_format.csv',fmt_rows); write_csv(str(prefix)+'_get_state.csv',state_rows); write_csv(str(prefix)+'_get_data_calls.csv',data_call_rows)
    write_csv(str(prefix)+'_get_data_axis.csv',axis_rows); write_csv(str(prefix)+'_d820.csv',d820_rows); write_csv(str(prefix)+'_ab9830.csv',ab9830_rows); write_csv(str(prefix)+'_pipeline.csv',pipeline_rows); write_csv(str(prefix)+'_raw_registration.csv',rawreg_rows); write_csv(str(prefix)+'_iat.csv',iat_rows)
    print(f'\nCSV prefix: {prefix}_*.csv')

if __name__=='__main__': main()
