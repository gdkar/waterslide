%thread(1) {
wsproto_in -r '../npu2_bench/packets.*.wsproto' -> $wsproto_data
$wsproto_data | bundle -N 256 |  workbalance -N 2 -J WCARDS -L CARD -> $wcard
#$wsproto_data | flush -N -t 2s -> $flush_var
}

%thread(9) {
pcap -r '../*.pcap' -> $pcap_data2
$pcap_data2   | bundle -N 256 |  workbalance -N 2 -J PCARDS -L CARD -> $pcard
#$pcap_data2   | flush -N -t 2s -> $flush_var
}

%thread(2) {
$wcard.CARD0 | workreceive -J WCARDS | unbundle -> $card0
$pcard.CARD0 | workreceive -J PCARDS | unbundle -> $card0
$card0 | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../bench_regexes.list -L NPU_MATCH_0 -m3 -M -v4 -> $npu_out0
}

%thread(3) {
$wcard.CARD1 | workreceive -J WCARDS | unbundle -> $card1
$pcard.CARD1 | workreceive -J PCARDS | unbundle -> $card1
$card1 | vectormatchnpu CONTENT -D /dev/lrl_npu1 -F ../bench_regexes.list -L NPU_MATCH_1 -m3 -M -v4 -> $npu_out1
}

%thread(6) {
$npu_out0,$npu_out1| labelstat | print -V
}
