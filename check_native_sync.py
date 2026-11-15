#!/usr/bin/env python3
"""Compile and run the actual backend's independently constructed wire cases."""
from pathlib import Path
import json,os,platform,shutil,subprocess,tempfile
from check_vif_restore import extract_function
ROOT=Path(__file__).resolve().parent
def run(source,output,cc=None):
    source=Path(source);output=Path(output);output.mkdir(parents=True,exist_ok=True)
    cc=cc or shutil.which('cc');env=os.environ.copy();env['UBSAN_OPTIONS']='halt_on_error=1'
    if platform.system()=='Linux':env['ASAN_OPTIONS']='detect_leaks=0'
    sdk=['-isysroot',subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()] if platform.system()=='Darwin' else []
    inc=source/'src/compat_rtw89';result={};scan_results={}
    with tempfile.TemporaryDirectory(prefix='owly-native-sync-') as t:
        temp=Path(t)
        glue=(inc/'rtw89_glue.c').read_text()
        names=('rtw89_glue_awdl_vif_enable','owly_sync_bound_channel','owly_link_update_locked',
               'owly_scan_protect_locked','rtw89_glue_awdl_scan_protect',
               'owly_sync_recheck','rtw89_glue_awdl_mi','owly_sync_work_run',
               'rtw89_glue_native_window','rtw89_glue_native_output','owly_sync_pause',
               'owly_set_sync_enabled','rtw89_glue_awdl_sync_enable',
               'rtw89_glue_awdl_vif_disable','rtw89_glue_awdl_sync_status')
        (temp/'production_scan.inc').write_text('\n\n'.join(extract_function(glue,name) for name in names))
        for mode in ['normal','asan_ubsan']:
            flags=['-O2'] if mode=='normal' else ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
            if mode=='asan_ubsan' and platform.system()=='Linux':flags.append('-no-pie')
            exe=Path(t)/mode
            args=[cc,*sdk,*flags,'-std=gnu11','-DOWLY_NATIVE_PEER_DATA=1','-I'+str(inc),*[str(inc/n) for n in ['owly_native_link.c','owly_awdl_sync.c','owly_native_config.c']],str(ROOT/'tests/native_sync_regression.c'),'-o',str(exe)]
            p=subprocess.run(args,capture_output=True,text=True,env=env);(output/(mode+'-compile.txt')).write_text(p.stdout+p.stderr)
            if p.returncode:raise RuntimeError('Native synchronization compile failed: '+mode)
            p=subprocess.run([str(exe)],capture_output=True,text=True,env=env);(output/(mode+'-run.txt')).write_text(p.stdout+p.stderr)
            if p.returncode:raise RuntimeError('Native synchronization regression failed: '+mode)
            result[mode]=p.stdout.strip()
            exe=temp/('scan-'+mode)
            args=[cc,*sdk,*flags,'-std=gnu11','-DOWLY_NATIVE_PEER_DATA=1','-DOWLY_AWDL_SOFTWARE_SYNC=1',
                  '-I'+str(inc),'-I'+str(temp),str(inc/'owly_native_link.c'),str(inc/'owly_awdl_sync.c'),
                  str(ROOT/'tests/scan_resume_model.c'),'-o',str(exe)]
            p=subprocess.run(args,capture_output=True,text=True,env=env)
            (output/('scan-'+mode+'-compile.txt')).write_text(p.stdout+p.stderr)
            if p.returncode:raise RuntimeError('AWDL scan model compile failed: '+mode)
            p=subprocess.run([str(exe)],capture_output=True,text=True,env=env)
            (output/('scan-'+mode+'-run.txt')).write_text(p.stdout+p.stderr)
            if p.returncode:raise RuntimeError('AWDL scan resume regression failed: '+mode)
            scan_results[mode]=p.stdout.strip()
    return {'actual_production_code':True,'independent_wire_fixture':True,'results':result,
            'scan_model_results':scan_results,'rf_or_airdrop_proven':False}
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=ROOT/'Source-Build');p.add_argument('--out',type=Path,default=ROOT/'validation/native-sync');a=p.parse_args()
    print(json.dumps(run(a.source,a.out),indent=2))
