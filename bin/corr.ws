%thread(1) {
pcap_in -r '../*.pcap' -> $wsproto_data
#wsproto_in -r '../npu2_bench/packets.cut*6.wsproto' -> $wsproto_data
#wsproto_in -r '../npu2_bench/packets.cut_pt006.wsproto' -> $wsproto_data
$wsproto_data | flush -N -t 1s -> $flush_data
#$wsproto_data | $npu_in
$flush_data:TAG, $wsproto_data:TAG | vectormatchnpu CONTENT -D /dev/lrl_npu0 -L NPU_MATCH -m16  -F ./expr.npup -v9  -> $npu_out
$flush_data:TAG, $npu_out:TAG | vectormatchre2 CONTENT   -L RE2_MATCH  -F ./expr.npup -> $re2_out
}
%thread(4) {
$re2_out.RE2_MATCH, $re2_out.NPU_MATCH | calc ' if (exists(RE2_MATCH) && !exists(NPU_MATCH)) then MISSED=1 ; endif; if (!exists(RE2_MATCH) && exists(NPU_MATCH)) then EXTRA=1 ; endif; if (exists(RE2_MATCH) != exists(NPU_MATCH)) then ERROR=1 ; endif; if (exists(RE2_MATCH)) then MATCHED=1 ; endif; ' -> $matched
$flush_data, $matched | labelstat -> $matched_stats
$flush_data, $re2_out | labelstat -> $re2_stats
$flush_data, $npu_out | bandwidth -> $npu_bandwidth
$flush_data, $re2_out | bandwidth -> $re2_bandwidth
$re2_bandwidth, $npu_bandwidth | print -V
$re2_stats,$matched_stats | print -VV
}
