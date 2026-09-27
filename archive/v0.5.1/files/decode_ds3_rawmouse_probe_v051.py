#!/usr/bin/env python3
"""Decode/analyse DS3_RawMouseProbe v0.5.1 binary capture.

This decoder does not infer camera rotation. It reports observed process Raw Input
API consumption and the parallel D820 diagnostic stream.
"""
from __future__ import annotations

import argparse
import csv
import math
import struct
from collections import Counter, defaultdict
from pathlib import Path

HEADER = struct.Struct("<8sIIIIqIIIIII8I")
RECORD_SIZE = 384
COMMON = struct.Struct("<qqIIIIII")

RECORD_MODULE = 1
RECORD_REG_CALL = 2
RECORD_REG_DEVICE = 3
RECORD_RAW_READ = 4
RECORD_PROCESSOR = 5
RECORD_STATS = 6
RECORD_META = 7
RECORD_IAT = 8
RECORD_RAW_BUFFER_CALL = 9
RECORD_RAW_BUFFER_MOUSE = 10

TYPE_NAMES = {
    RECORD_MODULE: "module",
    RECORD_REG_CALL: "reg_call",
    RECORD_REG_DEVICE: "reg_device",
    RECORD_RAW_READ: "raw_read",
    RECORD_PROCESSOR: "d820",
    RECORD_STATS: "stats",
    RECORD_META: "meta",
    RECORD_IAT: "iat",
    RECORD_RAW_BUFFER_CALL: "raw_buffer_call",
    RECORD_RAW_BUFFER_MOUSE: "raw_buffer_mouse",
}


def cstr(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("utf-8", "replace")


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def i32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<i", buf, off)[0]


def f32(buf: bytes, off: int) -> float:
    return struct.unpack_from("<f", buf, off)[0]


def parse_snapshot(buf: bytes, off: int) -> dict[str, int]:
    vals = struct.unpack_from("<11I B 3x", buf, off)
    keys = ["04","08","0c","10","14","18","1c","20","24","28","2c"]
    d = {f"bits_{k}": vals[i] for i, k in enumerate(keys)}
    d["flag_30"] = vals[-1]
    return d


def caller_label(base: int, rva: int, modules: dict[int, dict]) -> str:
    if not base:
        return f"unknown+0x{rva:08X}"
    m = modules.get(base)
    name = m["name"] if m else f"module@0x{base:08X}"
    return f"{name}+0x{rva:08X}"


def parse(path: Path):
    raw = path.read_bytes()
    if len(raw) < HEADER.size:
        raise SystemExit("Capture too small for v0.5.1 header")
    h = HEADER.unpack_from(raw, 0)
    magic, version, header_size, record_size, flags, freq, timestamp, image_size, text_rva, capacity, flush_ms, rescan_ms, *_ = h
    if magic != b"DS3RM51\0" or version != 51 or header_size != HEADER.size or record_size != RECORD_SIZE:
        raise SystemExit(f"Unsupported header: magic={magic!r} version={version} header={header_size} record={record_size}")
    payload = raw[header_size:]
    trailing = len(payload) % record_size
    if trailing:
        print(f"WARNING: {trailing} trailing byte(s); last record may be incomplete")
        payload = payload[: len(payload) - trailing]

    records = []
    counts = Counter()
    for off in range(0, len(payload), record_size):
        rec = payload[off:off+record_size]
        q0,q1,seq,typ,tid,caller_addr,caller_base,caller_rva = COMMON.unpack_from(rec, 0)
        row = {
            "qpc_begin": q0, "qpc_end": q1, "sequence": seq, "type": typ,
            "thread_id": tid, "caller_address": caller_addr,
            "caller_module_base": caller_base, "caller_rva": caller_rva,
            "_buf": rec,
        }
        records.append(row)
        counts[typ] += 1

    header = {
        "frequency": freq, "timestamp": timestamp, "image_size": image_size,
        "text_rva": text_rva, "capacity": capacity, "flush_ms": flush_ms,
        "rescan_ms": rescan_ms, "flags": flags,
    }
    return header, records, counts


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", nargs="?", type=Path, default=Path("DS3_RawMouseProbe.bin"))
    ap.add_argument("--out-prefix", type=Path)
    ns = ap.parse_args()
    header, records, counts = parse(ns.path)
    freq = header["frequency"]
    out = ns.out_prefix or ns.path.with_suffix("")

    modules: dict[int, dict] = {}
    for r in records:
        if r["type"] != RECORD_MODULE:
            continue
        b = r["_buf"]
        base = u32(b,40); size = u32(b,44)
        modules[base] = {"base":base,"size":size,"name":cstr(b[48:176]),"path":cstr(b[176:352])}

    def tsec(q: int) -> float:
        return q / freq if freq else 0.0

    print("DS3 Raw Mouse Probe v0.5.1")
    print(f"qpc_frequency={freq} timestamp=0x{header['timestamp']:08X} image_size=0x{header['image_size']:08X}")
    print(f"records={len(records)} types=" + ", ".join(f"{TYPE_NAMES.get(k,k)}:{v}" for k,v in sorted(counts.items())))
    print(f"modules_mapped={len(modules)}")

    # IAT installation diagnostics.
    iat_rows=[]
    for r in records:
        if r["type"] != RECORD_IAT: continue
        b=r["_buf"]; base=u32(b,40); rn=u32(b,44); gn=u32(b,48); bn=u32(b,52); cn=u32(b,56)
        iat_rows.append((base,rn,gn,bn,cn))
    if iat_rows:
        print("\nIAT observer:")
        for base,rn,gn,bn,cn in iat_rows:
            if rn or gn or bn or cn:
                print(f"  {modules.get(base,{}).get('name',f'0x{base:08X}')}: Register={rn} GetRawInputData={gn} GetRawInputBuffer={bn} conflicts={cn}")

    # Registration calls/devices.
    reg_calls={}
    reg_devices=defaultdict(list)
    for r in records:
        b=r["_buf"]
        if r["type"]==RECORD_REG_CALL:
            vals=struct.unpack_from("<12I",b,40)
            call_id,ptr,num,cb,captured,readable,result,error,*_=vals
            reg_calls[call_id]={**r,"ptr":ptr,"num":num,"cb":cb,"captured":captured,"readable":readable,"result":result,"error":error}
        elif r["type"]==RECORD_REG_DEVICE:
            vals=struct.unpack_from("<12I",b,40)
            call_id,index,total,up,usage,flags,target,result,error,*_=vals
            reg_devices[call_id].append({**r,"index":index,"total":total,"usage_page":up,"usage":usage,"flags":flags,"target":target,"result":result,"error":error})
    print(f"\nRegisterRawInputDevices calls={len(reg_calls)}")
    for cid in sorted(reg_calls):
        c=reg_calls[cid]; label=caller_label(c["caller_module_base"],c["caller_rva"],modules)
        print(f"  call={cid} t={tsec(c['qpc_begin']):.6f} tid={c['thread_id']} caller={label} entries={c['num']} captured={c['captured']} result={c['result']} error={c['error'] if not c['result'] else 0}")
        for d in sorted(reg_devices[cid],key=lambda x:x["index"]):
            print(f"    [{d['index']}] UsagePage={d['usage_page']} Usage={d['usage']} flags=0x{d['flags']:08X} hwndTarget=0x{d['target']:08X}")

    # Raw reads.
    raw_rows=[]
    by_caller=defaultdict(list)
    by_device=defaultdict(list)
    for r in records:
        if r["type"]!=RECORD_RAW_READ: continue
        b=r["_buf"]
        vals=struct.unpack_from("<19IiiI5I",b,40)
        (call_id,hraw,cmd,pdata,pcb,size_before,size_after,cbh,result,error,readable,valid,dwtype,dwsize,hdev,wparam,usflags,bflags,bdata,dx,dy,bytes_ret,*_) = vals
        rr={**r,"call_id":call_id,"hraw":hraw,"cmd":cmd,"pdata":pdata,"size_before":size_before,"size_after":size_after,"cbh":cbh,"result":result,"error":error,"readable":readable,"valid":valid,"dwtype":dwtype,"dwsize":dwsize,"hdev":hdev,"wparam":wparam,"usflags":usflags,"bflags":bflags,"bdata":bdata,"dx":dx,"dy":dy,"bytes":bytes_ret}
        raw_rows.append(rr)
        if valid:
            by_caller[(r["caller_module_base"],r["caller_rva"])].append(rr)
            by_device[hdev].append(rr)

    print(f"\nGetRawInputData actual RID_INPUT records={len(raw_rows)} valid_mouse={sum(r['valid'] for r in raw_rows)}")
    any_nonzero=False
    for key,rows in sorted(by_caller.items(), key=lambda kv:-len(kv[1])):
        nonzero=sum(1 for r in rows if not (r['usflags']&1) and (r['dx'] or r['dy']))
        any_nonzero |= nonzero>0
        first=min(r['qpc_begin'] for r in rows); last=max(r['qpc_begin'] for r in rows)
        devices=Counter(r['hdev'] for r in rows)
        sx=sum(r['dx'] for r in rows); sy=sum(r['dy'] for r in rows)
        print(f"  caller={caller_label(key[0],key[1],modules)} packets={len(rows)} nonzero={nonzero} duration={(last-first)/freq if freq else 0:.3f}s total_dx={sx} total_dy={sy} devices=" + ",".join(f"0x{k:08X}:{v}" for k,v in devices.items()))
    if by_device:
        print("  devices:")
        for dev,rows in sorted(by_device.items(), key=lambda kv:-len(kv[1])):
            nonzero=sum(1 for r in rows if not (r['usflags']&1) and (r['dx'] or r['dy']))
            print(f"    0x{dev:08X}: packets={len(rows)} nonzero={nonzero} total_dx={sum(r['dx'] for r in rows)} total_dy={sum(r['dy'] for r in rows)}")
    # GetRawInputBuffer calls and mouse packets.
    buffer_calls=[]
    buffer_mouse=[]
    buffer_by_caller=defaultdict(list)
    buffer_by_device=defaultdict(list)
    for r in records:
        b=r["_buf"]
        if r["type"]==RECORD_RAW_BUFFER_CALL:
            vals=struct.unpack_from("<20I",b,40)
            (call_id,pdata,pcb,size_before,size_after,cbh,result_count,error,readable,parsed,mouse_packets,nonzero_packets,wow64_layout,parse_errors,*_)=vals
            buffer_calls.append({**r,"call_id":call_id,"pdata":pdata,"size_before":size_before,"size_after":size_after,"cbh":cbh,"result_count":result_count,"error":error,"readable":readable,"parsed":parsed,"mouse_packets":mouse_packets,"nonzero_packets":nonzero_packets,"wow64_layout":wow64_layout,"parse_errors":parse_errors})
        elif r["type"]==RECORD_RAW_BUFFER_MOUSE:
            vals=struct.unpack_from("<10IiiI7I",b,40)
            (call_id,index,offset,packet_size,payload_offset,hdev,wparam,usflags,bflags,bdata,dx,dy,valid,*_)=vals
            rr={**r,"call_id":call_id,"index":index,"offset":offset,"packet_size":packet_size,"payload_offset":payload_offset,"hdev":hdev,"wparam":wparam,"usflags":usflags,"bflags":bflags,"bdata":bdata,"dx":dx,"dy":dy,"valid":valid}
            buffer_mouse.append(rr)
            if valid:
                buffer_by_caller[(r["caller_module_base"],r["caller_rva"])].append(rr)
                buffer_by_device[hdev].append(rr)

    print(f"\nGetRawInputBuffer calls={len(buffer_calls)} actual_calls={sum(1 for c in buffer_calls if c['pdata'])} returned_packets={sum(c['result_count'] for c in buffer_calls if c['result_count'] != 0xFFFFFFFF)} parsed_mouse={len(buffer_mouse)} parse_errors={sum(c['parse_errors'] for c in buffer_calls)}")
    if buffer_calls:
        for key,rows in sorted(defaultdict(list, {k:v for k,v in buffer_by_caller.items()}).items(), key=lambda kv:-len(kv[1])):
            nonzero=sum(1 for r in rows if not (r['usflags']&1) and (r['dx'] or r['dy']))
            any_nonzero |= nonzero>0
            devices=Counter(r['hdev'] for r in rows)
            print(f"  caller={caller_label(key[0],key[1],modules)} mouse_packets={len(rows)} nonzero={nonzero} total_dx={sum(r['dx'] for r in rows)} total_dy={sum(r['dy'] for r in rows)} devices=" + ",".join(f"0x{k:08X}:{v}" for k,v in devices.items()))
        if buffer_by_device:
            print("  buffer devices:")
            for dev,rows in sorted(buffer_by_device.items(), key=lambda kv:-len(kv[1])):
                nonzero=sum(1 for r in rows if not (r['usflags']&1) and (r['dx'] or r['dy']))
                print(f"    0x{dev:08X}: packets={len(rows)} nonzero={nonzero} total_dx={sum(r['dx'] for r in rows)} total_dy={sum(r['dy'] for r in rows)}")
    print("GAME_RAW_INPUT_CONFIRMED=" + ("YES" if any_nonzero else "NO"))

    # D820 summary.
    d820=defaultdict(list)
    d820_rows=[]
    for r in records:
        if r["type"]!=RECORD_PROCESSOR: continue
        b=r["_buf"]
        selfp=u32(b,40); word0=u32(b,44); word4=u32(b,48); dt_bits=u32(b,52)
        dt=f32(b,56); ix=f32(b,60); iy=f32(b,64); ox=f32(b,68); oy=f32(b,72)
        pre=parse_snapshot(b,76); post=parse_snapshot(b,124); result=i32(b,172)
        rr={**r,"self":selfp,"word0":word0,"word4":word4,"dt_bits":dt_bits,"dt":dt,"in_x":ix,"in_y":iy,"out_x":ox,"out_y":oy,"pre":pre,"post":post,"result":result}
        d820_rows.append(rr); d820[(r['caller_module_base'],r['caller_rva'])].append(rr)
    print(f"\nD820 records={len(d820_rows)}")
    for key,rows in sorted(d820.items(), key=lambda kv:-len(kv[1])):
        print(f"  caller={caller_label(key[0],key[1],modules)} records={len(rows)} max_abs_in={max(max(abs(r['in_x']),abs(r['in_y'])) for r in rows):.9g} max_abs_out={max(max(abs(r['out_x']),abs(r['out_y'])) for r in rows):.9g}")

    # Latest stats / integrity.
    stats=[]
    for r in records:
        if r["type"]!=RECORD_STATS: continue
        vals=struct.unpack_from("<26I",r["_buf"],40)
        keys=["enqueued","dropped","dropped_api","dropped_d820","highwater","write_errors","reg_calls","getraw_calls","getraw_query","getraw_actual","raw_packets","raw_nonzero","raw_errors","d820","modules","iat_reg","iat_raw","iat_buffer","iat_conflicts","getbuffer_calls","getbuffer_query","getbuffer_actual","buffer_packets","buffer_nonzero","buffer_parse_errors"]
        stats.append(dict(zip(keys,vals[:len(keys)])))
    if stats:
        s=stats[-1]
        print("\nLatest logger stats: " + " ".join(f"{k}={v}" for k,v in s.items()))
        print("CAPTURE_INTEGRITY=" + ("OK" if s["dropped"]==0 and s["write_errors"]==0 else "INVALID_OR_INCOMPLETE"))

    # CSV outputs.
    prefix=Path(str(out))
    with Path(str(prefix)+"_modules.csv").open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=["base","size","name","path"]);w.writeheader();
        for m in sorted(modules.values(),key=lambda x:x["base"]):w.writerow({"base":f"0x{m['base']:08X}","size":m['size'],"name":m['name'],"path":m['path']})
    with Path(str(prefix)+"_registrations.csv").open("w",newline="",encoding="utf-8") as f:
        fields=["time_s","call_id","thread_id","caller","num_devices","index","usage_page","usage","flags","hwnd_target","result","last_error"]
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for cid in sorted(reg_calls):
            c=reg_calls[cid]; devs=reg_devices[cid] or [None]
            for d in devs:
                w.writerow({"time_s":f"{tsec(c['qpc_begin']):.9f}","call_id":cid,"thread_id":c['thread_id'],"caller":caller_label(c['caller_module_base'],c['caller_rva'],modules),"num_devices":c['num'],"index":"" if d is None else d['index'],"usage_page":"" if d is None else d['usage_page'],"usage":"" if d is None else d['usage'],"flags":"" if d is None else f"0x{d['flags']:08X}","hwnd_target":"" if d is None else f"0x{d['target']:08X}","result":c['result'],"last_error":c['error'] if not c['result'] else 0})
    with Path(str(prefix)+"_raw_reads.csv").open("w",newline="",encoding="utf-8") as f:
        fields=["time_s","thread_id","caller","hrawinput","hdevice","us_flags","dx","dy","result","bytes","valid_mouse","size_before","size_after"]
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for r in raw_rows:w.writerow({"time_s":f"{tsec(r['qpc_begin']):.9f}","thread_id":r['thread_id'],"caller":caller_label(r['caller_module_base'],r['caller_rva'],modules),"hrawinput":f"0x{r['hraw']:08X}","hdevice":f"0x{r['hdev']:08X}","us_flags":f"0x{r['usflags']:04X}","dx":r['dx'],"dy":r['dy'],"result":r['result'],"bytes":r['bytes'],"valid_mouse":r['valid'],"size_before":r['size_before'],"size_after":r['size_after']})
    with Path(str(prefix)+"_raw_buffer_calls.csv").open("w",newline="",encoding="utf-8") as f:
        fields=["time_s","thread_id","caller","call_id","pdata","size_before","size_after","result_count","buffer_readable","parsed_packets","mouse_packets","nonzero_mouse_packets","wow64_layout","parse_errors","last_error"]
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for r in buffer_calls:w.writerow({"time_s":f"{tsec(r['qpc_begin']):.9f}","thread_id":r['thread_id'],"caller":caller_label(r['caller_module_base'],r['caller_rva'],modules),"call_id":r['call_id'],"pdata":f"0x{r['pdata']:08X}","size_before":r['size_before'],"size_after":r['size_after'],"result_count":r['result_count'],"buffer_readable":r['readable'],"parsed_packets":r['parsed'],"mouse_packets":r['mouse_packets'],"nonzero_mouse_packets":r['nonzero_packets'],"wow64_layout":r['wow64_layout'],"parse_errors":r['parse_errors'],"last_error":r['error'] if r['result_count']==0xFFFFFFFF else 0})
    with Path(str(prefix)+"_raw_buffer_mouse.csv").open("w",newline="",encoding="utf-8") as f:
        fields=["time_s","thread_id","caller","call_id","packet_index","packet_offset","packet_size","payload_offset","hdevice","us_flags","dx","dy","valid_mouse"]
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for r in buffer_mouse:w.writerow({"time_s":f"{tsec(r['qpc_begin']):.9f}","thread_id":r['thread_id'],"caller":caller_label(r['caller_module_base'],r['caller_rva'],modules),"call_id":r['call_id'],"packet_index":r['index'],"packet_offset":r['offset'],"packet_size":r['packet_size'],"payload_offset":r['payload_offset'],"hdevice":f"0x{r['hdev']:08X}","us_flags":f"0x{r['usflags']:04X}","dx":r['dx'],"dy":r['dy'],"valid_mouse":r['valid']})
    with Path(str(prefix)+"_d820.csv").open("w",newline="",encoding="utf-8") as f:
        fields=["time_begin_s","time_end_s","thread_id","caller","self","dt","in_x","in_y","out_x","out_y","result"]
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for r in d820_rows:w.writerow({"time_begin_s":f"{tsec(r['qpc_begin']):.9f}","time_end_s":f"{tsec(r['qpc_end']):.9f}","thread_id":r['thread_id'],"caller":caller_label(r['caller_module_base'],r['caller_rva'],modules),"self":f"0x{r['self']:08X}","dt":r['dt'],"in_x":r['in_x'],"in_y":r['in_y'],"out_x":r['out_x'],"out_y":r['out_y'],"result":r['result']})
    print(f"\nCSV prefix: {prefix}_*.csv")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
