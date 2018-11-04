%thread(1) {
wsproto_in -r ../npu2_bench/packets.cut*.wsproto -> $data_in
#pcap -r ../*.pcap  -r ../*.pcap -r ../*.pcap -> $data_in
$data_in | workbalance -N 2 -J CARDS -L CARD -> $card
#$data_in | flush -N -t 1s -> $flush_var
}
%thread(2) {
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../bench_regexes.list -L NPU_MATCH -m3 -M -> $npu_out
}
%thread(3) {
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu1 -F ../bench_regexes.list -L NPU_MATCH -m3 -M -> $npu_out
}
%thread(6) {
$npu_out | bandwidth
}
