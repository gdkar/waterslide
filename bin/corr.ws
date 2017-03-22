%thread(1) {
#pcap_in -r '../npu2_bench/nitroba.pcap' -> $wsproto_data
#wsproto_in -r '../npu2_bench/packets.cut*6.wsproto' -> $wsproto_data
wsproto_in -r '../npu2_bench/packets.cut_pt006.wsproto' -> $wsproto_data
$wsproto_data | flush -N -t 1s -> $flush_data
#$wsproto_data | $npu_in
$flush_data:TAG, $wsproto_data:TAG | vectormatchnpu CONTENT -D /dev/lrl_npu0 -M -L NPU_MATCH -m16  -F ./temp.npup -v9  -> $npu_out
#}
#%thread(3) {
#$npu_out | unbundle -> $re2_in
$flush_data:TAG, $npu_out:TAG | vectormatchre2 CONTENT  -L RE2_MATCH  -F ./temp.npup  -> $re2_out
#}
#%thread(4) {
$re2_out | calc ' if (exists(RE2_MATCH) && !exists(NPU_MATCH)) then MISSED=1 ; endif; if (!exists(RE2_MATCH) && exists(NPU_MATCH)) then EXTRA=1 ; endif; if (exists(RE2_MATCH) != exists(NPU_MATCH)) then ERROR=1 ; endif; if (exists(RE2_MATCH)) then MATCHED=1 ; endif; ' -> $matched
$matched | labelstat | print -V
$re2_out | labelstat
}